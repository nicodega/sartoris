/*
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


#include "layout.h"
#include "formats/elf.h"
#include "formats/initfs2.h"
#include "elf_loader.h"
#include "loader.h"
#include "task_thread.h"
#include "types.h"
#include "io.h"
#include "pman_print.h"
#include "kmalloc.h"
#include "helpers.h"
#include <sartoris/syscall.h>
#include <services/stds/stdfss.h>
#include <services/pmanager/services.h>

struct ifs2_header *iheader = NULL;
/* Main STDFSS service */
extern UINT16 filesystem_service;
/* HDD Service */
extern UINT16 atac_service;

UINT32 curr_cursor;
struct ifs2srv_header *curr_header;

ADDR create_service(UINT16 task, UINT16 thread, INT32 invoke_level, UINT32 size, BOOL low_mem, BOOL load_all, char *image_name);
UINT32 put_pages(struct pm_task *task, BOOL use_fsize, BOOL low_mem);
INT32 pminit_elf_open(int imgId);
INT32 pminit_elf_seek(struct fsio_event_source *iosrc, UINT32 offset);
INT32 pminit_elf_read(struct fsio_event_source *iosrc, UINT32 size, ADDR pbuffer);
INT32 get_img_header(struct ifs2srv_header **header, int imgId);

void loader_init(ADDR init_image_laddress)
{
	UINT32 addr = 0;
	UINT32 imsg[4];
	UINT32 i = 0;
	UINT16 task, thread;
	BOOL msg, lowmem;
	iheader = (struct ifs2_header *)init_image_laddress;

	pman_print_clr(12);
	pman_print("InitFS2 Services: %i, Magic: %x ", iheader->entries, iheader->ifs2magic);
	pman_print_set_color(7);

	/* Load services on init fs */
	for(i = 0; i < iheader->entries; i++)
	{
		// Open next image.
		if(pminit_elf_open(i))
			pman_print_and_stop("Could not open service %i of %i ", i, iheader->entries);
		
		msg = 0;
		lowmem = 0;
		
		if(!(curr_header->flags & IFS2SRV_FLAG_IGNORE))
		{
			if(curr_header->main_thread != 0xFFFF)
				thread = curr_header->main_thread;
			else
				thread = thr_get_id(1, MAX_THR);

			if(curr_header->task != 0xFFFF)
				task = curr_header->task;
			else
				task = tsk_get_id(MAX_TSK,MAX_TSK);
			
			if(curr_header->flags & IFS2SRV_FLAG_LOWMEM)
			{
				lowmem = 1;
				pman_print(" *Service will be loaded on low-memory area.");
			}
			if(curr_header->pman_type & IFS2SRV_PMTYPE_MAINFS)
			{
				filesystem_service = task;
				pman_print(" *Identified Main FS (STDFSS) Service");
			}
			if(curr_header->pman_type & IFS2SRV_PMTYPE_HDD)
			{
				atac_service = task;
				pman_print(" *Identified HDD (ATAC) Service");
			}
		
			pman_print("Loading service %s task: %x thread: %x , header thr %x ", curr_header->img_name, task, thread, curr_header->main_thread);

			addr = (UINT32)create_service(task, thread, 0, PMAN_TASK_SIZE, lowmem, TRUE, curr_header->img_name);		
		}
	}
}

// slot_size MUST be a 4k multiple
// Creates a system service, based on the initfs image.
// Returns address of first page assigned to the task.
ADDR create_service(UINT16 task, UINT16 thread, INT32 invoke_level, UINT32 size, BOOL low_mem, BOOL load_all, char *image_name)
{
	struct pm_task *ptask = NULL;
	struct pm_thread *pthread = NULL;
	UINT32 psize = 0, first_page, i = 0;
	char *path = NULL;
	struct vmm_page_table *ptbl = NULL;
	struct thread mk_thread;

	while(image_name[psize] != '\0'){ psize++; }

	path = kmalloc(psize);

	if(path == NULL)
		pman_print_and_stop("Could not allocate memory for path task: %s", image_name);
	
	while(image_name[i] != '\0'){ path[i] = image_name[i]; i++; }
	path[i] = '\0';
	
	// Create a service task
	ptask = tsk_create(task);
    if(ptask == NULL)
        pman_print_and_stop("Error allocating task for %s", image_name);

	if(loader_create_task(ptask, path, psize, 1, TRUE) != PM_OK)
		pman_print_and_stop("Error creating task for %s", image_name);
	
	/* 
	Create task gave us a page directory, the first page table, and initialized task structure as a service. 
	But since sysservice is TRUE, it did not begin fetching from FS.
	*/
	ptask->flags = 0;
	ptask->flags |= TSK_FLAG_SYS_SERVICE;

    if(low_mem)
        ptask->flags |= TSK_LOW_MEM;

	/* Setup the task */
	ptask->command_inf.creator_task_id = 0xFFFF;
	ptask->command_inf.response_port = 0xFFFF;
	ptask->command_inf.req_id = 0;
	ptask->command_inf.command_sender_id = 0xFFFFFFFF;

	/* Parse elf */
	if(elf_begin(ptask, pminit_elf_read, pminit_elf_seek) == -1)
		pman_print_and_stop("Elf parsing failed for %s", image_name);
	
	/* Setup first thread */
	pthread = thr_create(thread, ptask);
    	
	pthread->state = THR_WAITING;
		
	/* Create microkernel thread */
	mk_thread.task_num = task;
	mk_thread.invoke_mode = PRIV_LEVEL_ONLY;
	mk_thread.invoke_level = 0;
	mk_thread.ep = (ADDR)ptask->loader_inf.elf_header.e_entry;
	mk_thread.stack = pthread->stack_addr = (ADDR)STACK_ADDR(PMAN_THREAD_STACK_BASE);

	if(create_thread(thread, &mk_thread))
		pman_print_and_stop("Could not create thread for %s", image_name);

	/* Put pages for the Service */
	put_pages(ptask, !load_all, low_mem);

	/* Get first page */
	ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(ptask->vmm_inf.page_directory->tables[PM_LINEAR_TO_DIR(SARTORIS_PROCBASE_LINEAR)].b));
	first_page = PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(SARTORIS_PROCBASE_LINEAR)].entry.phy_page_addr);
	
	/* Schedule and activate thread */
	sch_add(pthread);
	sch_activate(pthread);

	ptask->state = TSK_NORMAL;

	return (ADDR)first_page;
}

/* This function will pm_page_in loadable segments. If use_fsize is non zero, then it'll only 
load as much as there is on the file, else, it'll load the whole segment.
This function will return the ammount of bytes allocated for pages (not tables). Return value
will be a multiple of page size. */
UINT32 put_pages(struct pm_task *task, BOOL use_fsize, BOOL low_mem)
{
	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;
	UINT32 i = 0, j;
	UINT32 allocated = 0;
	struct Elf32_Phdr *prog_header = NULL;
	UINT32 page_addr, pagecount, foffset;
	ADDR pg_tbl, pg, pgp;
	
	/* Put Pages in for loadable segments */
	while( i < task->loader_inf.elf_header.e_phnum)
	{
		prog_header = (struct Elf32_Phdr*)&task->loader_inf.elf_pheaders[i * task->loader_inf.elf_header.e_phentsize];

		if(prog_header->p_type == PT_LOAD)
		{
			page_addr = (unsigned int)prog_header->p_vaddr - ((unsigned int)prog_header->p_vaddr % 0x1000);
			pagecount = 0;
			foffset = prog_header->p_offset;

			page_addr += SARTORIS_PROCBASE_LINEAR;	// our elf files will begin at 0, so add the base

			if(use_fsize)
				pagecount = (UINT32)(prog_header->p_filesz / 0x1000) + ((prog_header->p_filesz % 0x1000 == 0)? 0 : 1);
			else
				pagecount = (UINT32)(prog_header->p_memsz / 0x1000) + ((prog_header->p_memsz % 0x1000 == 0)? 0 : 1);
			
			/* Load as many pages as needed. */
			for(j = 0; j < pagecount; j++)
			{
				// if page table is not present, add it
				if(pdir->tables[PM_LINEAR_TO_DIR(page_addr)].ia32entry.present == 0)
				{
					if(page_addr == 0x800000) pman_print("Loading page table for address %x ", page_addr);

					/* Insert a page for the page table */
					// low mem is only considered for pages, not here
					// because it does not make any difference 
					pg_tbl =  (ADDR)vmm_get_tblpage(task->id, page_addr);

					if(page_in(task->id, (ADDR)page_addr, (ADDR)LINEAR2PHYSICAL(pg_tbl), 1, PGATT_WRITE_ENA) != SUCCESS)
						pman_print_and_stop("Failed to page_in for table laddress: %x, physical: %x ", page_addr, pg);

					task->vmm_inf.page_count++;
				}
                
				pg = (ADDR)LINEAR2PHYSICAL(vmm_get_page_ex(task->id, page_addr, low_mem));
				
				pgp = (ADDR)PHYSICAL2LINEAR(pg);

				/* Set page as service (won't be paged out) */
				vmm_set_flags(task->id, pgp, TRUE, TAKEN_EFLAG_SERVICE, TRUE);
                                
				if(foffset < curr_header->image_size)
				{
					pminit_elf_seek(&task->io_event_src, foffset);
					pminit_elf_read(&task->io_event_src, PAGE_SIZE, pgp);					
				}
			
				if(page_in(task->id, (ADDR)page_addr, (ADDR)pg, 2, PGATT_WRITE_ENA) != SUCCESS)
					pman_print_and_stop("Failed to page_in for laddress: %x, physical: %x ", page_addr, pg);
				
                /* unmap the page from pmanager, so it's assigned record is used. */
                vmm_unmap_page(task->id, (ADDR)page_addr);

				task->vmm_inf.page_count++;
			
				allocated += PAGE_SIZE;
				page_addr += PAGE_SIZE;
				foffset += PAGE_SIZE;
			}
		}
		i++;
	}
	
	return allocated;
}

INT32 strcmp(char *c1, char *c2)
{
	INT32 i = 0;
	while(c1[i] == c2[i] && c1[i] != '\0' && c2[i] != '\0'){ i++; }

	return c1[i] == c2[i];
}

/* Simulated filesystem for elf loading */
INT32 pminit_elf_open(int imgId)
{
	if(get_img_header(&curr_header, imgId)) return 1; // header not found  

	curr_cursor = 0;

	return 0;
}

/* functions for io on elf parsing */
INT32 pminit_elf_seek(struct fsio_event_source *iosrc, UINT32 offset) 
{
	curr_cursor = offset;

	return 1;
}

INT32 pminit_elf_read(struct fsio_event_source *iosrc, UINT32 size, ADDR pbuffer)
{
	UINT32 i = 0;
	char *ptr = (char*)curr_header->image_pos;
	char *buffer = (char*)pbuffer;

	while(i < size && curr_cursor + i < curr_header->image_size)
	{ 
		buffer[i] = ptr[curr_cursor+i]; 
		i++;
	}
	
	curr_cursor += i;

	return 1;
}

/* Fills the given header with img header, and returns img possition in memory */
INT32 get_img_header(struct ifs2srv_header **header, int imgId)
{
	struct ifs2srv_header *c_header = (struct ifs2srv_header *)((UINT32)iheader + iheader->headers);
	UINT32 i = 0;

	while(i < iheader->entries && i != imgId)
	{
		c_header++;
		i++;
	}

	if(i == iheader->entries) return 1;

	*header = c_header;
	(*header)->image_pos += (UINT32)iheader; // this can be done only because this function is issued only once

	
	return 0;
}





