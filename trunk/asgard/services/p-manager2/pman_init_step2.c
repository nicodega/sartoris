
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "layout.h"
#include "formats/bootinfo.h"
#include "types.h"
#include "mem_layout.h"
#include "vm.h"
#include "helpers.h"
#include "loader.h"
#include "interrupts.h"
#include "kmalloc.h"
#include "command.h"
#include "scheduler.h"
#include "pman_print.h"
#include "formats/initfs2.h"

void process_manager();
void init_reloc();

/*
	ok, this is pman init stage two. we will execute this code, and then jump to the process 
	manager main processing loop.
	
	What we will do here, is setup the page pool. And initialize System services, along with structures.
	Notice, we are now task 0 on the system.
*/	
void pman_init_stage2()
{
	UINT32 linear, physical; 
	struct pm_thread *pmthr = NULL;
	struct pm_task *pmtsk = NULL;
	int i = 0;

	/*
		//////////////// IMPORTANT NOTE ///////////////

		DO NOT UNDER ANY CIRCUNSTANCE PRINT OR ACCESS
		GLOBAL VARIABLES UNTIL INIT IMAGE HAS BEEN 
		COPIED!! IT WOULD DESTROY IMAGE DATA

		///////////////////////////////////////////////
	*/

	/* get rid of the init stuff */
	destroy_thread(INIT_THREAD_NUM);
	destroy_task(INIT_TASK_NUM);
	
	/*
	Open used ports
	*/
	for(i = 0; i <= 12; i++)
	{
		open_port(i, 3, PRIV_LEVEL_ONLY);
	}
	
	/* 
		Init stage 1 has placed bootinfo at PMAN_MULTIBOOTINFO_PHYS 
		before initializing the pool we need to know memory size
		and that information is there. So lets map it on our page table.
	*/
	linear = PMAN_MULTIBOOT_LINEAR + SARTORIS_PROCBASE_LINEAR;
  	physical = PMAN_MULTIBOOT_PHYS; 

	map_pages(PMAN_TASK, linear, physical, PMAN_MULTIBOOT_PAGES, PGATT_WRITE_ENA, 2);

	/* Reallocate init image */
	init_reloc();

	/* NOTE: Now it's safe to print, for we moved the init image */

	pman_print("Mapping Malloc %i pages", PMAN_MALLOC_PAGES);
		
	/* Pagein remaining pages for kmalloc */
	linear = PMAN_MALLOC_LINEAR + SARTORIS_PROCBASE_LINEAR; // place after multiboot
  	physical = PMAN_MALLOC_PHYS; 

	map_pages(PMAN_TASK, linear, physical, PMAN_MALLOC_PAGES, PGATT_WRITE_ENA, 2);

	pman_print("Initializing tasks/threads.");
	
	tsk_init();
	thr_init();

	if(((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->flags & MB_INFO_MMAP && ((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_length > 0)
	{
		/* 
		Calculate multiboot mmap linear address.
		Sartoris loader left MMAP just after multiboot info structure.
		*/
		((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_addr = PMAN_MULTIBOOT_LINEAR + sizeof(struct multiboot_info);

		pman_print("Multiboot MMAP Size: %i ", ((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_length);
		pman_print("Multiboot mmap linear address: %x", ((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_addr);

		struct mmap_entry *entry = NULL;
		entry = (struct mmap_entry *)((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_addr;

		int kk = 0, mmlen = ((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR)->mmap_length / entry->size;
		for(kk = 0; kk < mmlen; kk++)
		{
			pman_print("Multiboot entry size: %i start: %x end: %x type: %i", entry->size, (UINT32)entry->start, (UINT32)entry->end, entry->type);		

			entry = (struct mmap_entry *)((UINT32)entry + entry->size);
		}
	}
	else
	{
		pman_print("No MMAP present.");
	}
	
	/* Initialize vmm subsystem */
	vmm_init((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR, PMAN_INIT_RELOC_PHYS, PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE);
	
	/* Mark SCHED_THR as taken! */
	pmthr = thr_get(SCHED_THR);
	pmthr->state = THR_INTHNDL;		// ehm... well... it IS an interrupt handler :D
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	

	pmtsk = tsk_get(PMAN_TASK);
	pmtsk->state = TSK_NORMAL;

	pman_print("Initializing allocator and interrupts.");

	/* Initialize kernel memory allocator */
	kmem_init(PMAN_MALLOC_LINEAR, PMAN_MALLOC_PAGES);
	
	/* get our own interrupt handlers, override microkernel defaults */
	int_init();
	
	/* Initialize Scheduler subsystem */
	sch_init();

	pman_print("InitFS2 Service loading...");
	
	/* Load System Services and init Loader */
	loader_init((ADDR)PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS));

	pman_print_clr(7);
	pman_print("Loading finished, return INIT image memory to POOL...");

	/* Put now unused Init-Fs pages onto vmm managed address space again. */
	vmm_add_mem((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR
				,PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS)
				,PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE));
	
	pman_print("Signals Initialization...");

	/* Initialize global signals container */
	init_signals();

	pman_print("Commands Initialization...");

	/* Initialize Commands subsystem. */
	cmd_init();

	pman_print_set_color(12);
	pman_print("PMAN: Initialization step 2 completed.");

	/* Create Scheduler int handler */
	if(create_int_handler(32, SCHED_THR, FALSE, 0) < 0)
		pman_print_and_stop("Could not create Scheduler thread.");
	
	/* This is it, we are finished! */
	process_manager();
}

void init_reloc()
{
	UINT32 i, physical, left, physicaldest; 
	char *dest, *src;
	struct ifs2_header *h = NULL;
	
	/* 
		Now before we initialize vmm Module we have to copy init files 
	   image to PMAN_INIT_RELOC_PHYS on physical memory, so 
	   pool page tables won't overlap 
	*/

	// test for ifs2
	page_in(PMAN_TASK, (ADDR) (PMAN_STAGE2_MAPZONE_SOURCE + SARTORIS_PROCBASE_LINEAR), (ADDR)(PMAN_SARTORIS_INIT_PHYS + PMAN_SIZE), 2, PGATT_WRITE_ENA);
	h = (struct ifs2_header*)PMAN_STAGE2_MAPZONE_SOURCE;
	
	if(h->ifs2magic != IFS2MAGIC)
		pman_print_and_stop("Init Fs 1 format is no longer supported by PMAN");
	
	// ifs2 header found, check for LZW and decompress if necesary!
	if(h->flags & IFS2_FLAG_LZW)
	{
		/* NOT IMPLEMENTED: I won't implement it until I test the rest of PMAN */
	}
	else
	{
		pman_print("Uncompressed InitFS2 Found: Size: %d (hardcoded)", PMAN_INITIMG_SIZE);

		// uncompressed IFS
		left = PMAN_INITIMG_SIZE;

		/* Sartoris left the image at position PMAN_SARTORIS_INIT_PHYS 
		and we want it on PMAN_INIT_RELOC_PHYS. PMAN_INIT_RELOC_PHYS is greater
		than PMAN_SARTORIS_INIT_PHYS. we will copy 4kb at the time from 
		bottom up, this means PMAN_INIT_RELOC_PHYS - PMAN_SARTORIS_INIT_PHYS has to be
		greater or equal than 0x1000. */
		physical = PMAN_SARTORIS_INIT_PHYS + PMAN_SIZE + PMAN_INITIMG_SIZE - 0x1000;	// start copy at the last page
		physicaldest = PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE - 0x1000;				// destination
		
		dest = (char*)PMAN_STAGE2_MAPZONE_DEST;
		src = (char*)PMAN_STAGE2_MAPZONE_SOURCE;
			
		do
		{
			// map source
			page_in(PMAN_TASK, (ADDR) (PMAN_STAGE2_MAPZONE_SOURCE + SARTORIS_PROCBASE_LINEAR), (ADDR) physical, 2, PGATT_WRITE_ENA);
			// map dest
			page_in(PMAN_TASK, (ADDR) (PMAN_STAGE2_MAPZONE_DEST + SARTORIS_PROCBASE_LINEAR), (ADDR) physicaldest, 2, PGATT_WRITE_ENA);

			// copy bytes
			i = 0x1000;
			while(i > 0){dest[i-1] = src[i-1]; i--; }

			left -= 0x1000;
			physicaldest -= PAGE_SIZE;
			physical -= PAGE_SIZE;
			
		}while(left > 0);
	}	
}

