/*
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

#include "initfs.h"
#include <sartoris/kernel.h>
#include "pmanager_internals.h"
#include <os/layout.h>

extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

// slot_size MUST be a 4k multiple
unsigned int create_service(int task, int thread, int invoke_level, char *image_name, unsigned int size, unsigned int forced_physical, int load_all)
{
	struct task srv;
	struct thread thr;
	int i, j;
	unsigned int linear;
	int pages;

	// get header info
	if(pminit_elf_open(image_name))
	{
		string_print("header not found",0,12);
		STOP;
	}

	/* Setup the task */
	task_info[task].state = TSK_NORMAL;
	task_info[task].flags = TSK_FLAG_SYS_SERVICE;
	task_info[task].fs_task_id = -1;
	task_info[task].open_fd = -1;
	task_info[task].first_thread = thread;
	task_info[task].num_threads = 1;
	task_info[task].creator_task_id = -1;
	task_info[task].response_port = -1;
	task_info[task].req_id = -1;
	task_info[task].destroy_sender_id = -1;
	task_info[task].swap_free_addr = SARTORIS_PROCBASE_LINEAR;
	task_info[task].swap_page_count = task_info[task].page_count = 0;

	thread_info[thread].task_id = task;
	thread_info[thread].state = THR_WAITING;
	thread_info[thread].flags = 0;
	thread_info[thread].fault_address = 0;
	thread_info[thread].page_in_address = 0;
	thread_info[thread].fault_smo_id = -1;
	thread_info[thread].next_thread = -1;
	thread_info[thread].fault_next_thread = -1;
	thread_info[thread].swaptbl_next = -1;
	thread_info[thread].stack_addr = (void*)STACK_ADDR(PM_THREAD_STACK_BASE);

	/* Parse elf */
	if(elf_begin(task, pminit_elf_seek, pminit_elf_read) == -1)
	{
		string_print("elf parsing failed",0,12);
		STOP;
	}

	/* load the page directory */
	task_info[task].page_dir = pm_get_page();

	struct taken_entry tentry;

	CREATE_TAKEN_DIR_ENTRY(&tentry, task);
	pm_assign(task_info[task].page_dir, PMAN_TBL_ENTRY_NULL, &tentry );

	pm_page_in(task, (void*)0, task_info[task].page_dir, 0, 0);
	
	unsigned int allocated = put_pages(task, !load_all, (void*)forced_physical);

	/* create task and thread */
	srv.mem_adr = (void*)SARTORIS_PROCBASE_LINEAR;  //  virtual
	srv.size    = size; 
	srv.priv_level = 0;

	if (create_task(task, &srv, (void*) NO_TASK_INIT, 0) < 0) STOP;

	thr.task_num = task;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = invoke_level;
	thr.ep = task_info[task].elf_header.e_entry;
	thr.stack = (void*)STACK_ADDR(PM_THREAD_STACK_BASE);

	if (create_thread(thread, &thr) < 0) STOP;

	return allocated;
}

unsigned int curr_cursor;
struct service_header curr_header;

/* This function will pm_page_in loadable segments. If use_fsize is non zero, then it'll only 
load as much as there is on the file, else, it'll load the whole segment.
This function will return the ammount of bytes allocated for pages (not tables). Return value
will be a multiple of page size. */
unsigned int put_pages(int task, int use_fsize, void *phys_start)
{
	unsigned int *pgdir = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(task_info[task].page_dir));
	int i = 0, j;
	unsigned int physical = (unsigned int)phys_start;
	unsigned int allocated = 0;
	struct taken_entry tentry;

	char str[128]; // REMOVE
	
	/* Put Pages in for loadable segments */
	while( i < task_info[task].elf_header.e_phnum)
	{
		struct Elf32_Phdr *prog_header = (struct Elf32_Phdr*) &task_info[task].elf_pheaders[i*task_info[task].elf_header.e_phentsize];

		if(prog_header->p_type == PT_LOAD)
		{
			unsigned int page_addr = (unsigned int)prog_header->p_vaddr - ((unsigned int)prog_header->p_vaddr % 0x1000);
			unsigned int pagecount = 0;
			unsigned int foffset = prog_header->p_offset;

			page_addr += SARTORIS_PROCBASE_LINEAR;	// our elf files will begin at 0, so add the base

			if(use_fsize)
			{
				pagecount = (int)(prog_header->p_filesz / 0x1000) + ((prog_header->p_filesz % 0x1000 == 0)? 0 : 1);
			}
			else
			{
				pagecount = (int)(prog_header->p_memsz / 0x1000) + ((prog_header->p_memsz % 0x1000 == 0)? 0 : 1);
			}
		
			for(j = 0; j < pagecount; j++)
			{
				// if page table is not present, add it
				if(!(PG_PRESENT(pgdir[PM_LINEAR_TO_DIR(page_addr)])))
				{
					/* Insert a page for the page table */
					// physical is only considered for pages, not here
					// because it does not make any difference 
					void *pg_tbl =  (void *)pm_get_page();

					CREATE_TAKEN_TBL_ENTRY(&tentry, task, PM_LINEAR_TO_DIR(page_addr), TAKEN_EFLAG_SERVICE);

					pm_assign(pg_tbl, PMAN_TBL_ENTRY_NULL, &tentry );

					page_in(task, (void*)page_addr, pg_tbl, 1, PGATT_WRITE_ENA);

					task_info[task].page_count++;
				}

				void *pg = 0;
				if(physical != 0)
				{
					pg = (void *)physical;
				}
				else
				{
					pg = pm_get_page();
					
				}

				char *pgp = 0;
				if(foffset < curr_header.image_size)
				{
					pgp = (char*)PHYSICAL2LINEAR(pg);
					pminit_elf_seek(task, foffset);
					pminit_elf_read(task, PAGE_SIZE, pgp);
				}
			
				page_in(task, (void*)page_addr, (void *)pg, 2, PGATT_WRITE_ENA);

				task_info[task].page_count++;
			
				CREATE_TAKEN_PG_ENTRY(&tentry, 0, PM_LINEAR_TO_TAB(page_addr), TAKEN_EFLAG_SERVICE);
				pm_assign(pg, CREATE_PMAN_TBL_ENTRY(task, PM_LINEAR_TO_DIR(page_addr), 0 ), &tentry );

				if(physical != 0)
				{
					physical += PAGE_SIZE;
				}

				allocated += PAGE_SIZE;

				page_addr += PAGE_SIZE;
				foffset += PAGE_SIZE;
			}
		}
		i++;
	}
	return allocated;
}

int strcmp(char *c1, char *c2)
{
	int i = 0;
	while(c1[i] == c2[i] && c1[i] != '\0' && c2[i] != '\0')
	{
		i++;
	}

	return c1[i] == c2[i];
}

void pm_init_tasks() {

  int i;

  for (i=0; i<MAX_TSK; i++) {
    
    task_info[i].state = TSK_NOTHING;
    task_info[i].full_path[0] = '\0';
    task_info[i].open_fd = -1;
    task_info[i].first_thread = -1;
    task_info[i].num_threads = 0;
    task_info[i].flags = 0;

  }

}

void pm_init_threads() {

  int i;

  for (i=0; i<MAX_THR; i++) {

    thread_info[i].state = THR_NOTHING;
    thread_info[i].next_thread = -1;

  }
}

/* Simulated filesystem for elf loading */
int pminit_elf_open(char *imgname)
{
	if(get_img_header(&curr_header, imgname)) return 1; // header not found  

	curr_cursor = 0;
	return 0;
}

/* functions for io on elf parsing */
int pminit_elf_seek(int task_id, unsigned int offset) 
{
	curr_cursor = offset;

	return 1;
}

int pminit_elf_read(int task_id, int size, char *buffer)
{
	unsigned int i = 0;
	char *ptr = (char*)curr_header.image_pos;

	while(i < size && curr_cursor + i < curr_header.image_size){ buffer[i] = ptr[curr_cursor+i]; i++;}

	curr_cursor += i;

	return 1;
}

/* Fills the given header with img header, and returns img possition in memory */
int get_img_header(struct service_header *header, char *img_name)
{
	struct init_header *iheader = (struct init_header *)PMAN_INITSTAGE2_LINEAR;
	struct service_header *c_header = (struct service_header *)(PMAN_INITSTAGE2_LINEAR + sizeof(struct init_header));
	unsigned int i = 0;

	while(i < iheader->services_count && !strcmp(c_header->image_name, img_name))
	{
		c_header++;
		i++;
	}

	if(i == iheader->services_count) return 1;

	header->image_size = c_header->image_size;
	header->image_pos = c_header->image_pos + PMAN_INITSTAGE2_LINEAR;

	return 0;
}


