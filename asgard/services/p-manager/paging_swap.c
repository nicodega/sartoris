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

#include <os/layout.h>
#include "pmanager_internals.h"
#include "page_list.h"
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/atac/atac_ioctrl.h>

/* Page alocation IO initialization.
*/
extern int atac_finddev_smo;
extern struct atac_find_dev_param atacparams;
extern int stdfs_task_id;

void pa_page_taken()
{
	if(pa_curr_mem <= pa_min_mem)
	{
		pa_curr_mem -= 0x1000;
	}
	else if(pa_curr_mem - 0x1000 <= pa_min_mem)
	{
		min_hits++;	// ups got to min hits again... damit!
	}
}

void pa_page_added()
{
	unsigned int freedmem = 0x1000;
	
	if(pa_curr_mem <= pa_min_mem && (pa_curr_mem + freedmem > pa_min_mem + PA_MEMPAROLE))
	{
		min_hits = 0;
	}

	pa_curr_mem += 0x1000;
}

/* If a page requires IO operation for loading, this will return 1. */
int check_page_swap(int task_id, int thread_id, void *linear)
{
	struct stdblockdev_readm_cmd readcmd;
	struct taken_entry tentry;
	int curr_thr;

	/* Check task table present bit. */
	unsigned int **pdir = (unsigned int **)PHYSICAL2LINEAR(PG_ADDRESS(task_info[task_id].page_dir));

	thread_info[thread_id].fault_entry = 0xFFFFFFFF;

	/* 
		If table is not present, it might be on swap or not.
		If it's not on swap we won't do anything. Else, we will fetch it.
		NOTE: for us it's important to keep page tables on swap file, because 
		they keep information on pages which have been sent to disk. (the cost for lesser metadata) 
	*/

	/*
		NOTE: here we must be very careful. other thread might have page faulted on a different 
		linear address, but on the same page table and it will be being retrieved.
		If so, we will add this thread to the thread list (a thread list for page tables, not the same used for pages).
	*/

	if(PA_TASKTBL_SWAPPED(pdir[PM_LINEAR_TO_DIR(linear)]) )
	{
		/* Check if page fault is on a table currently being used by 
		other page fault IO. (this prevents removing IO lock from
		a table while pfaults are still being served) */

		curr_thr = task_info[thread_info[thread_id].task_id].first_thread;

		while(curr_thr != -1)
		{
			if( curr_thr != thread_id && (thread_info[curr_thr].flags & THR_FLAG_PAGEFAULT) && thread_info[curr_thr].swaptbl_next == -1 && PM_LINEAR_TO_DIR(thread_info[curr_thr].fault_address) == PM_LINEAR_TO_DIR(linear) )
			{
				/* found last thread waiting for this page table */
				thread_info[curr_thr].swaptbl_next = thread_id;	// set next thread as our current thread
				break;
			}

			curr_thr = thread_info[curr_thr].next_thread;
		}	

		/* Put the thread on hold for both the table and the page. */
		thread_info[thread_id].state = THR_BLOCKED;
		thread_info[thread_id].flags |= THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL;
		thread_info[thread_id].fault_address = linear;
		thread_info[thread_id].read_size = 0;
		thread_info[thread_id].page_perms = 0;
		thread_info[thread_id].page_displacement = 0;
		thread_info[thread_id].fault_next_thread = -1;
		thread_info[thread_id].swaptbl_next = -1;

		if(curr_thr != -1)
		{
			/* Page or table is swapped an it must be retrieved from disk. */
			return 1;	
		}
		
		/* begin IO read operation, for the page table. */
		thread_info[thread_id].page_in_address = pm_get_page();

		CREATE_TAKEN_TBL_ENTRY(&tentry, task_id, PM_LINEAR_TO_DIR( linear ), TAKEN_EFLAG_IOLOCK | ((task_info[task_id].flags & TSK_FLAG_SERVICE)? TAKEN_EFLAG_SERVICE : 0));
		pm_assign(thread_info[thread_id].page_in_address, PMAN_TBL_ENTRY_NULL, &tentry );

		thread_info[thread_id].fault_smo_id = share_mem(stdfs_task_id, PHYSICAL2LINEAR(thread_info[thread_id].page_in_address), 0x1000, WRITE_PERM);
		
		/* Send read multiple command to ATAC */
		readcmd.command = BLOCK_STDDEV_READM;
		readcmd.pos = page_stealing_swapstart + PA_TASKTBL_SWAPADDR(pdir[PM_LINEAR_TO_DIR(linear)]) / 0x200;
		readcmd.count = 8;
		readcmd.dev = page_stealing_ldevid;
		readcmd.buffer_smo = thread_info[thread_id].fault_smo_id;
		readcmd.msg_id = thread_id;
		readcmd.ret_port = SWAP_READ_PORT;

		send_msg(ATAC_TASK, 4, &readcmd);

		/* Page or table is swapped an it must be retrieved from disk. */
		return 1;
	}
	else if(!PG_PRESENT(pdir[PM_LINEAR_TO_DIR(linear)]))
	{
		/* table is not present, nor swapped => page its not on swap. */
		return 0;
	}

	/* Page table is present and not swapped, check record on task page table. */
	//unsigned int tblphys = PG_ADDRESS(pdir[PM_LINEAR_TO_DIR(linear)]);			
	unsigned int *tbl = (unsigned int *)PHYSICAL2LINEAR(PG_ADDRESS(pdir[PM_LINEAR_TO_DIR(linear)]));	

	/* Get the record from pman table. */
	unsigned int entry = tbl[PM_LINEAR_TO_TAB(linear)];

	/* Check whether page is on swap file. */
	if(!PA_TASKTBL_SWAPPED(entry)) return 0;

	/* Check if page fault is on a table currently being used by 
		other page fault IO. (this prevents removing IO lock from
		a table while pfaults are still being served) */

	curr_thr = task_info[thread_info[thread_id].task_id].first_thread;

	while(curr_thr != -1)
	{
		if( curr_thr != thread_id && (thread_info[curr_thr].flags & THR_FLAG_PAGEFAULT) && thread_info[curr_thr].swaptbl_next == -1 && PM_LINEAR_TO_DIR(thread_info[curr_thr].fault_address) == PM_LINEAR_TO_DIR(linear) )
		{
			/* found last thread waiting for this page table */
			thread_info[curr_thr].swaptbl_next = thread_id;	// set next thread as our current thread
			break;
		}

		curr_thr = thread_info[curr_thr].next_thread;
	}	

	/* Lock page table for IO, and set PF */
	struct taken_entry *ptentry = get_taken((void*)PG_ADDRESS(pdir[PM_LINEAR_TO_DIR(linear)]));

	ptentry->data.b_ptbl.eflags |= TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF;

	/* if page was sent to swap, begin swap IO */
	thread_info[thread_id].state = THR_BLOCKED;
	thread_info[thread_id].flags |= THR_FLAG_PAGEFAULT;
	thread_info[thread_id].fault_address = linear;
	thread_info[thread_id].read_size = 0;
	thread_info[thread_id].page_perms = 0;
	thread_info[thread_id].page_displacement = 0;
	thread_info[thread_id].fault_next_thread = -1;
	thread_info[thread_id].swaptbl_next = -1;
	thread_info[thread_id].fault_entry = entry;

	thread_info[thread_id].page_in_address = pm_get_page();

	CREATE_TAKEN_PG_ENTRY(&tentry, 0, PM_LINEAR_TO_TAB( linear ),  TAKEN_EFLAG_IOLOCK | ((task_info[task_id].flags & TSK_FLAG_SERVICE)? TAKEN_EFLAG_SERVICE : 0));
	pm_assign(thread_info[thread_id].page_in_address , PMAN_TBL_ENTRY_NULL, &tentry );

	thread_info[thread_id].fault_smo_id = share_mem(stdfs_task_id, PHYSICAL2LINEAR(thread_info[thread_id].page_in_address), 0x1000, WRITE_PERM);
	
	/* Send read multiple command to ATAC */
	readcmd.command = BLOCK_STDDEV_READM;
	readcmd.pos = page_stealing_swapstart + PA_TASKTBL_SWAPADDR(entry) / 0x200;
	readcmd.count = 8;
	readcmd.dev = page_stealing_ldevid;
	readcmd.buffer_smo = thread_info[thread_id].fault_smo_id;
	readcmd.msg_id = thread_id;
	readcmd.ret_port = SWAP_READ_PORT;

	send_msg(ATAC_TASK, 4, &readcmd);

	return 1;
}

void pa_check_swap_msg()
{
	struct stdblockdev_res res;
	int id;
	unsigned int *pgdir;
	struct taken_entry *tentry;

	while(get_msg_count(SWAP_DESTROY_READ_PORT) > 0)
	{
		get_msg(SWAP_DESTROY_READ_PORT, &res, &id);

		if(id == ATAC_TASK && res.ret != STDBLOCKDEV_ERR && task_info[res.msg_id].state == TSK_KILLED)
		{
			pgdir = (unsigned int*)PHYSICAL2LINEAR(task_info[res.msg_id].page_dir);

			swap_free_addr( PA_TASKTBL_SWAPADDR(pgdir[PM_LINEAR_TO_DIR(task_info[res.msg_id].swap_free_addr)]) );

			task_swap_empty((int)res.msg_id);
		}
	}

	while(get_msg_count(SWAP_READ_PORT) > 0)
	{
		get_msg(SWAP_READ_PORT, &res, &id);

		if(id == ATAC_TASK && res.ret != STDBLOCKDEV_ERR)
		{
			/* Got a successfull response for a read command */		
			int thread_id = res.msg_id;
			int task_id = thread_info[thread_id].task_id;

			/* 
				This read command might have raised because 
				a table was being read for a page 
			*/
			if(thread_info[thread_id].flags & THR_FLAG_PAGEFAULT_TBL)
			{
				/* Set swap address for the page as free (before page in) */
				pgdir = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(task_info[task_id].page_dir));

				swap_free_addr( PA_TASKTBL_SWAPADDR(pgdir[PM_LINEAR_TO_DIR(thread_info[thread_id].fault_address)]) );

				/* page in, and process each PF */
				pm_page_in(task_id, thread_info[thread_id].fault_address, thread_info[thread_id].page_in_address, 1, PGATT_WRITE_ENA);

				tentry = get_taken((void*)PG_ADDRESS(thread_info[thread_id].page_in_address));

				/* Set page taken status on our tables */
				pm_assign((void*)PG_ADDRESS(thread_info[thread_id].page_in_address), PMAN_TBL_ENTRY_NULL, tentry );

				claim_mem(thread_info[thread_id].fault_smo_id);

				thread_info[thread_id].flags &= ~THR_FLAG_PAGEFAULT_TBL;
				thread_info[thread_id].fault_smo_id = -1;

				/* Process page faults */

				/* Process threads waiting for this page table */
				int curr_thr = thread_info[thread_id].swaptbl_next;
				int bcurr;

				task_info[task_id].page_count++;
				task_info[task_id].swap_page_count--;
				
				while(curr_thr != -1)
				{
					thread_info[curr_thr].fault_smo_id = -1;
					thread_info[curr_thr].flags &= ~THR_FLAG_PAGEFAULT_TBL;

					/* 
						If thread was not waiting for the same page, process 
						it's page fault. 
					*/
					if(PG_ADDRESS(thread_info[thread_id].fault_address) != PG_ADDRESS(thread_info[curr_thr].fault_address))
					{
						bcurr = thread_info[curr_thr].swaptbl_next;

						if(!pm_handle_page_fault(&curr_thr, 1))
						{
							/* Wake all pending threads */
							wake_pf_threads(curr_thr);
						}
					}

					curr_thr = bcurr;
				}

				/* Process our page fault */
				if(pm_handle_page_fault(&thread_id, 1))
					continue;

				/* Wake all pending threads */
				wake_pf_threads(thread_id);
			}
			else
			{
				/* Set swap address for the page as free (before page in) */
				swap_free_addr( PA_TASKTBL_SWAPADDR(thread_info[thread_id].fault_entry) );
			
				/* Page has been fetched from swap */
				claim_mem(thread_info[thread_id].fault_smo_id);

				/* page in, and process each PF */
				pm_page_in(thread_info[thread_id].task_id, thread_info[thread_id].fault_address, thread_info[thread_id].page_in_address, 2, PGATT_WRITE_ENA);

				/* Set assigned record for the page */
				pgdir = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(task_info[thread_info[thread_id].task_id].page_dir));
				unsigned int tbl_addr = PG_ADDRESS( pgdir[PM_LINEAR_TO_DIR(thread_info[thread_id].fault_address)] );

				tentry = get_taken((void*)PG_ADDRESS(thread_info[thread_id].page_in_address));

				tentry->data.b_pg.eflags &= ~TAKEN_EFLAG_IOLOCK;

				/* Set page taken status on our tables */
				pm_assign((void*)PG_ADDRESS(thread_info[thread_id].page_in_address), CREATE_PMAN_TBL_ENTRY(task_id, PM_LINEAR_TO_DIR(thread_info[thread_id].fault_address), 0 ), tentry );

				claim_mem(thread_info[thread_id].fault_smo_id);

				thread_info[thread_id].flags &= ~THR_FLAG_PAGEFAULT;
				thread_info[thread_id].fault_smo_id = -1;
				
				task_info[task_id].page_count++;
				task_info[task_id].swap_page_count--;

				/* Wake all pending threads */
				wake_pf_threads(thread_id);
			}
		}
	}
}

/* set swap address as free on the bitmap */
void swap_free_addr(unsigned int swap_addr)
{
	swap_addr = swap_addr / 0x1000;

	unsigned int slinaddr = swap_addr / 8;

	unsigned int pindex = slinaddr / 0x1000;
	unsigned int index = slinaddr % 0x1000;
	unsigned int shift = 7 - (swap_addr % 8);

	swapbitmap.storage->addr[pindex][index] = swapbitmap.storage->addr[pindex][index] & ~(0x1 << shift);

	if(swapbitmap.storage_free > slinaddr)
	{
		swapbitmap.storage_free = slinaddr;
	}
}

/* set swap address as taken on the bitmap */
void swap_use_addr(unsigned int swap_addr)
{
	swap_addr = swap_addr / 0x1000;

	unsigned int slinaddr = swap_addr / 8;

	unsigned int pindex = slinaddr / 0x1000;
	unsigned int index = (slinaddr % 0x1000);
	int shift = 7 - (swap_addr % 8);

	swapbitmap.storage->addr[pindex][index] = swapbitmap.storage->addr[pindex][index] | (0x1 << shift);
	
	if(swapbitmap.storage_free == slinaddr)
	{
		while(swapbitmap.storage->addr[pindex][index] == 0xFF)
		{
			index++;
			if(index == 0x1000)
			{
				index = 0;
				pindex++;
				if(pindex == swapbitmap.storage_pages)
				{
					swapbitmap.storage_free = 0xFFFFFFFF;
					return;
				}
			}
		}
		swapbitmap.storage_free = pindex * 0x1000 + index;
	}
}

/* Get a free address on swap file */
unsigned int swap_get_addr()
{
	if(swapbitmap.storage_free == 0xFFFFFFFF) return 0xFFFFFFFF;

	unsigned int pindex = swapbitmap.storage_free / 0x1000;
	unsigned int index = swapbitmap.storage_free % 0x1000;
	int i = 7;

	while(i >= 0)
	{
		if( !(swapbitmap.storage->addr[pindex][index] & (0x1 << i)) ) break;
	}

	return (pindex * 0x1000 + index) * 0x1000;
}

/* Frees all pages on swap file for a given task. (ASYNCH) */
void task_swap_empty(int task_id)
{
	struct pm_msg_response pm_res;
	struct pm_msg_finished pm_finished;
	int creator_task_id = -1;
	int response_port = -1;
	int req_id = -1;

	/* Check directory entries looking for swapped tables */
	unsigned int *pdir = (unsigned int*)PHYSICAL2LINEAR(task_info[task_id].page_dir);

	int i = task_info[task_id].swap_free_addr / 0x400000;

	while(task_info[task_id].swap_free_addr < PM_MAPPING_BASE && task_info[task_id].swap_page_count > 0)
	{
		if( PA_TASKTBL_SWAPPED(pdir[i]) )
		{
			/* Table is swapped, get it */		
			struct stdblockdev_readm_cmd readcmd;

			task_info[task_id].task_smo_id =  share_mem(stdfs_task_id, PHYSICAL2LINEAR(PG_ADDRESS(pdir[i])), 0x1000, WRITE_PERM);

			/* Send read multiple command to ATAC */
			readcmd.command = BLOCK_STDDEV_READM;
			readcmd.pos = page_stealing_swapstart + PA_TASKTBL_SWAPADDR(pdir[i]) / 0x200;
			readcmd.count = 8;
			readcmd.dev = page_stealing_ldevid;
			readcmd.buffer_smo = task_info[task_id].task_smo_id;
			readcmd.msg_id = task_id;
			readcmd.ret_port = SWAP_DESTROY_READ_PORT;

			send_msg(ATAC_TASK, 4, &readcmd);

			task_info[task_id].swap_page_count--;

			return;	// this function will continue when atac read messages are read.
		}
		else if( PG_PRESENT(pdir[i]) )
		{
			/* Check table entries. */
			unsigned int *pgtbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(pdir[i]));
			int j = 0;

			for(j = 0; j < 1024; j++)
			{
				if(PA_TASKTBL_SWAPPED(pgtbl[j]))
				{
					swap_free_addr( PA_TASKTBL_SWAPADDR(pgtbl[PM_LINEAR_TO_TAB(task_info[task_id].swap_free_addr + j * 0x1000)]) );
				}
			}
		}

		i++;
		task_info[task_id].swap_free_addr += 0x400000;
	}

	/* Finished! */
	// file closed successfully, swap freed, destroy the task
	creator_task_id = task_info[task_id].creator_task_id;
	response_port   = task_info[task_id].response_port;
	req_id          = task_info[task_id].req_id;

	pm_destroy_task(task_id);

	// if there is a destroy sender, send an ok to the 
	// task
	if( task_info[task_id].destroy_sender_id != -1)
	{
		struct pm_msg_response msg_ans;
			
		msg_ans.pm_type = PM_DESTROY_TASK;
		msg_ans.req_id  = task_info[task_id].destroy_req_id;
		msg_ans.status  = PM_TASK_OK;
		msg_ans.new_id  = task_id;
		msg_ans.new_id_aux = -1;
			
		send_msg(task_info[task_id].destroy_sender_id, task_info[task_id].destroy_ret_port, &msg_ans);
	}

	if(creator_task_id != -1)
	{
		// inform the creating task 
		pm_finished.pm_type = PM_TASK_FINISHED;
		pm_finished.req_id =  req_id;
		pm_finished.taskid = task_id;
		pm_finished.ret_value = task_info[task_id].ret_value;

		send_msg(creator_task_id, response_port, &pm_finished);
	}
}





