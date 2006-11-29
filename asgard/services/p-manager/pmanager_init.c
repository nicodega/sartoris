/*
*	This function will be the init service entry point. Once it has been runned by sartoris,
*	we will create our main task (the pman task), algong with necesary pages, and run ourselves.
*
*	Process and Memory Manager Service
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/


#include <sartoris/kernel.h>
#include "pmanager_internals.h"
#include <os/layout.h>
#include "page_list.h"
#include "bootinfo.h"


extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];
extern page_list free_pages;

void pman_init()
{
	struct task srv;
	struct thread thr;
	unsigned int pages;
	
	__asm__ ("cli" : :) ;

	/*
		Sartoris left thigs this way:
			- 0x100000 end of microkernel space (sartoris loader left bootinfo struct right here!)
			- 0x800000 this is were we are running

		What we will do is:

			- Create a page directory on 0x100000
			- Create a table for the pman process to fit in. (4MB)
			- Create a new task, for the process manager main.
			- Create a new thread for the pman, with ep to pmain_init_stage2().
			- Run the thread.
	*/

	/* Copy multiboot info left by sartoris loader to PMAN_MULTIBOOT_PHYS 
	   NOTE: Sartoris has mapped 4mb of our linear address space. Bootinfo is placed 
	   at PMAN_SARTORIS_MMAP_LINEAR in our space :), but we have to be careful, because that area
	   is mapped at PMAN_SARTORIS_MMAP_PHYS on physical memory.
	   What I'll do is map from PMAN_STAGE1_MAPZONE (it's safe for the init stack is placed at
	   0x3F0000 - 0x4) onto PMAN_MULTIBOOT_PHYS and copy exactly 64kb.
	*/
	int i;
	unsigned int linear = PMAN_STAGE1_MAPZONE + SARTORIS_PROCBASE_LINEAR;	
  	unsigned int physical = PMAN_MULTIBOOT_PHYS; 

	for(i=0; i < PMAN_MULTIBOOT_PAGES; i++) {
  		page_in(INIT_TASK_NUM, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
		linear += PAGE_SIZE;
		physical += PAGE_SIZE; 
  	}

	/* Now copy boot info :) */
	int *dest = (int*)PMAN_STAGE1_MAPZONE, *src = (int*)PMAN_SARTORIS_MMAP_LINEAR;
	for(i=0; i<(PMAN_MULTIBOOT_SIZE / 4);i++){ dest[i] = src[i]; }

	/* Create page dir */
	page_in(PMAN_TASK, (void*)0, (void*)(PMAN_PDIR_PHYS), 0, 0);

	/* Create a page table */
	/* PMAN should fit on 4MB */
	page_in(PMAN_TASK, (void*)SARTORIS_PROCBASE_LINEAR, (void*)(PMAN_PTBL_PHYS), 1, PGATT_WRITE_ENA);

	/* Now the pages, starting at 0x100000 + 0x2000 */
	linear = SARTORIS_PROCBASE_LINEAR;
  	physical = PMAN_CODE_PHYS; // avoid stepping on the page dir/table

	for(i=0; i < PMAN_CODE_PAGES; i++) 
	{
  		page_in(PMAN_TASK, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
		linear += PAGE_SIZE;
		physical += PAGE_SIZE; 
  	}

	/* Create the task */
	srv.mem_adr = (void*)SARTORIS_PROCBASE_LINEAR;       // virtual
	srv.size    = 0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR; 
	srv.priv_level = 0;			// service

	if (create_task(PMAN_TASK, &srv, (void*) 0, PMAN_SIZE) < 0) STOP;

	/* Create the thread */
	thr.task_num = PMAN_TASK;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 3;
	thr.ep = &pman_init_stage2;
	thr.stack = (void *)STACK_ADDR(PMAN_STACK_ADDR);

	if (create_thread(SCHED_THR, &thr) < 0) STOP;

	/* Run the thread */
	run_thread(SCHED_THR); 
	STOP;
}

void calc_mem(struct multiboot_info *mbinf);

void pman_init_stage2()
{
	/*
		ok, this is pman init stage two. we will execute this code, and then jump to the process 
		manager main processing loop.
		
		What we will do here, is setup the page pool. And initialize System services, along with structures.

		Notice, we are now task 0 on the system.
	*/	

	/* get rid of the init stuff */
	destroy_thread(INIT_THREAD_NUM);
	destroy_task(INIT_TASK_NUM);

	/* Init stage 1 has placed bootinfo at PMAN_MULTIBOOTINFO_PHYS 
	   before initializing the pool we need to know memory size
	   and that information is there. So lets map it on our page table.
	*/
	int i;
	unsigned int linear = PMAN_MULTIBOOT_LINEAR + SARTORIS_PROCBASE_LINEAR;
  	unsigned int physical = PMAN_MULTIBOOT_PHYS; 

	for(i=0; i < PMAN_MULTIBOOT_PAGES; i++) {
  		page_in(PMAN_TASK, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
		linear += PAGE_SIZE;
		physical += PAGE_SIZE; 
  	}

	/* Now before we initialize the pool we have to copy init files 
	   image to PMAN_INIT_RELOC_PHYS on physical memory, so 
	   pool page tables won't overlap */
	unsigned int left = PMAN_INITIMG_SIZE;

	/* Sartoris left the image at position PMAN_SARTORIS_INIT_PHYS 
	and we want it on PMAN_INIT_RELOC_PHYS. PMAN_INIT_RELOC_PHYS is greater
	than PMAN_SARTORIS_INIT_PHYS. we will copy 4kb at the time from 
	bottom up, this means PMAN_INIT_RELOC_PHYS - PMAN_SARTORIS_INIT_PHYS as to be
	greater or equal than 0x1000. */
	physical = PMAN_SARTORIS_INIT_PHYS + PMAN_SIZE + PMAN_INITIMG_SIZE - 0x1000;	// start copy at the last page
	unsigned int physicaldest = PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE - 0x1000;	// destination
	
	char *dest = (char*)PMAN_STAGE2_MAPZONE_DEST;
	char *src = (char*)PMAN_STAGE2_MAPZONE_SOURCE;
		
	do
	{
		// map source
		page_in(PMAN_TASK, (void*) PMAN_STAGE2_MAPZONE_SOURCE + SARTORIS_PROCBASE_LINEAR, (void*) physical, 2, PGATT_WRITE_ENA);
		// map dest
		page_in(PMAN_TASK, (void*) PMAN_STAGE2_MAPZONE_DEST + SARTORIS_PROCBASE_LINEAR, (void*) physicaldest, 2, PGATT_WRITE_ENA);

		// copy bytes
		i = 0x1000;
		while(i > 0){dest[i-1] = src[i-1]; i--; }

		left -= 0x1000;
		physicaldest -= PAGE_SIZE;
		physical -= PAGE_SIZE;
		
	}while(left > 0);

	/* Pagein malloc pages */
	linear = PMAN_MALLOC_LINEAR + SARTORIS_PROCBASE_LINEAR; // place after multiboot
  	physical = PMAN_MALLOC_PHYS; 

	for(i=0; i < PMAN_MALLOC_PAGES; i++) 
	{	
		page_in(PMAN_TASK, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
		linear += PAGE_SIZE;
		physical += PAGE_SIZE; 
  	}

	/* Now calculate memory size based on multiboot info */
	calc_mem((struct multiboot_info*)PMAN_MULTIBOOT_LINEAR);

	/* First initialize the pool */
	// 0x100000 bytes (1MB) wont be placed on the pool so we give some space to dmaman
	pm_init_page_pool((void *)PMAN_INIT_RELOC_PHYS, (void *)(PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE + 0x100000)); 

	pm_init_tasks();
	pm_init_threads();

	/* Mark SCHED_THR as taken! */
	thread_info[SCHED_THR].state = THR_INTHNDL;	// ehm... well... it IS an interrupt handler :D
	thread_info[SCHED_THR].task_id = PMAN_TASK;
	thread_info[EXC_HANDLER_THR].state = THR_INTHNDL;	
	thread_info[EXC_HANDLER_THR].task_id = PMAN_TASK;
	task_info[PMAN_TASK].state = TSK_NORMAL;

	/* Setup paging thread info */
	thread_info[PAGEAGING_THR].task_id = PMAN_TASK;
	thread_info[PAGEAGING_THR].state = THR_INTERNAL;
	thread_info[PAGEAGING_THR].flags = 0;
	
	thread_info[PAGESTEALING_THR].task_id = PMAN_TASK;
	thread_info[PAGESTEALING_THR].state = THR_INTERNAL;
	thread_info[PAGESTEALING_THR].flags = 0;

	/* Initialize pman malloc */
	init_mem((char*)PMAN_MALLOC_LINEAR, PMAN_MALLOC_SIZE);

	/* get our own interrupt handlers, override microkernel defaults */
	pm_spawn_handlers();

	/* Create System Services */
	unsigned int allocated = pm_spawn_services();

	/* Now feed the pool with init pages */
	unsigned int end = PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE);
	unsigned int offset = 0;

	for (offset = PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS); offset < end; offset+=PAGE_SIZE) 
	{      
      	put_page(&free_pages, (void*) (offset));
		pa_page_added();
	}

	end = PHYSICAL2LINEAR(PMAN_LOWMEM_PHYS + 0x100000);
	
	for (offset = PHYSICAL2LINEAR(PMAN_LOWMEM_PHYS + allocated); offset < end; offset+=PAGE_SIZE) 
	{   
      	put_page(&free_pages, (void*) (offset));
		pa_page_added();
	}

	/* Create int handler */
	if(create_int_handler(32, SCHED_THR, false, 0) < 0) STOP;

	init_signals();

	/* This is it, we are finished! */
	process_manager();
}

void calc_mem(struct multiboot_info *mbinf)
{
	physical_memory = PMAN_DEFAULT_MEM_SIZE;

	// check mbinf flags
	if(mbinf->flags & MB_INFO_MEM)
	{
		physical_memory = (mbinf->mem_upper + 1024)*1024;	// mem upper is memory above the MB and is in kb
	}
	
	if(mbinf->flags & MB_INFO_MMAP)
	{
		// FIXME: we should try and improve the value on mem_upper here...
	}

	/* substract from available physical memory what we will use for pman */
	physical_memory -= PMAN_POOL_PHYS;
	

	// now lets calculate pool megabytes
	/* Pool MB must be a 4MB multiple */
	pool_megabytes = (physical_memory / 0x100000) - ((physical_memory / 0x100000) % 4);
}

