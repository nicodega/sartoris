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


#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "layout.h"
#include "mem_layout.h"
#include "vm.h"
#include "swap.h"
#include "task_thread.h"
#include "exception.h"
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/atac/atac_ioctrl.h>
#include <services/pmanager/services.h>
#include "pman_print.h"
#include "helpers.h"

/*
This variables will be used for aging/stealing synchronization.
*/
UINT32 curr_st_region;	// region currently being "stealed"
UINT32 curr_ag_region;	// region currently being "aged"

/*
Check if the page table is swapped
*/
BOOL vmm_check_swap_tbl(struct pm_task *task, struct pm_thread *thread, ADDR pg_laddr)
{
	struct pm_thread *curr_thr;

	/* Check task table present bit. */
	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;

	if(pdir->tables[PM_LINEAR_TO_DIR(pg_laddr)].record.swapped == 1)
	{
		/* 
		Check if page fault is on a table currently being used by 
		other page fault IO. (this prevents removing IO lock from
		a table while pfaults are still being served) 
		*/

		curr_thr = task->first_thread;

		while(curr_thr != NULL)
		{
			if( curr_thr != thread && (curr_thr->flags & THR_FLAG_PAGEFAULT) && curr_thr->vmm_info.swaptbl_next == NULL && PM_LINEAR_TO_DIR(curr_thr->vmm_info.fault_address) == PM_LINEAR_TO_DIR(pg_laddr) )
			{
				/* found last thread waiting for this page table */
				curr_thr->vmm_info.swaptbl_next = thread;		// set next thread as our current thread
				break;
			}

			curr_thr = curr_thr->next_thread;
		}	

		/* Put the thread on hold for both the table and the page. */
        sch_deactivate(thread);

		thread->state = THR_BLOCKED;
		thread->flags |= THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL;
		thread->vmm_info.fault_address = pg_laddr;
		thread->vmm_info.read_size = 0;
		thread->vmm_info.page_perms = 0;
		thread->vmm_info.page_displacement = 0;
		thread->vmm_info.fault_next_thread = NULL;
		thread->vmm_info.swaptbl_next = NULL;

		if(curr_thr != NULL)
		{
			return TRUE;	
		}
		
		/* begin IO read operation, for the page table. */
		thread->vmm_info.page_in_address = vmm_get_page(task->id, PG_ADDRESS(pg_laddr));

		/* IO lock page table, and set PF */
		vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(task->id, pg_laddr)), TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

		/* 
		LBA is calculated based on the not present record index. This index
		is stored in pages multiples.
		*/
		thread->swp_io_finished.callback = swap_table_read_callback;
		io_begin_pg_read( (pdir->tables[PM_LINEAR_TO_DIR(pg_laddr)].record.addr << 3), thread->vmm_info.page_in_address, thread);
		
		/* Page or table is swapped an it must be retrieved from disk. */
		return TRUE;
	}
	return FALSE;
}

/* 
Check if requested page is swapped, and begin swap fetch if so. 
Returns: If TRUE, page is swapped and fetch has begun. FALSE otherwise. 
*/
BOOL vmm_check_swap(struct pm_task *task, struct pm_thread *thread, ADDR pg_laddr)
{
	struct pm_thread *curr_thr;
	struct vmm_page_table *tbl;
	struct vmm_not_present_record *np_record;

	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;

	thread->vmm_info.fault_entry.addr = 0xFFFFF;
	thread->vmm_info.fault_entry.present = 1;
	thread->vmm_info.fault_entry.swapped = 1;
	thread->vmm_info.fault_entry.unused = 1;
	/* 
		If table is not present, it might be on swap or not.
		If it's not on swap we won't do anything. Else, we will fetch it.
		NOTE: for us it's important to keep page tables on swap file, because 
		they keep information on pages which have been sent to disk. (the cost for lesser metadata) 
	*/

	/*
		NOTE: here we must be very careful. other thread might have page faulted on a different 
		pg_laddr address, but on the same page table and it will be being retrieved.
		If so, we will add this thread to the thread list (a thread list for page tables, not the same used for pages).
	*/

	if(pdir->tables[PM_LINEAR_TO_DIR(pg_laddr)].record.present != 1)
	{
		/* table is not present, nor swapped => page its not on swap. */
		return FALSE;
	}

	/* Page table is present and not swapped, check record on task page table. */
	tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(pg_laddr)].b));	

	/* Get the record from pman table. */
	np_record = &tbl->pages[PM_LINEAR_TO_TAB(pg_laddr)].entry.record;

	/* Check whether page is on swap file. */
	if(np_record->swapped != 1) return FALSE;

	/* 
	Check if page fault is on a table currently being used by 
	other page fault IO. (this prevents removing IO lock from
	a table while pfaults are still being served) 
	*/

	curr_thr = task->first_thread;

	while(curr_thr != NULL)
	{
		if( curr_thr != thread && (curr_thr->flags & THR_FLAG_PAGEFAULT) && curr_thr->vmm_info.swaptbl_next == NULL && PM_LINEAR_TO_DIR(curr_thr->vmm_info.fault_address) == PM_LINEAR_TO_DIR(pg_laddr) )
		{
			/* found last thread waiting for this page table */
			curr_thr->vmm_info.swaptbl_next = thread;	// set next thread as our current thread
			break;
		}

		curr_thr = curr_thr->next_thread;
	}	

	/* IO lock page table */
	vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(task->id, pg_laddr)), TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

	/* if page was sent to swap, begin swap IO */
    sch_deactivate(thread);

	thread->state = THR_BLOCKED;
	thread->flags |= THR_FLAG_PAGEFAULT;
	thread->vmm_info.fault_address = pg_laddr;
	thread->vmm_info.read_size = 0;
	thread->vmm_info.page_perms = 0;
	thread->vmm_info.page_displacement = 0;
	thread->vmm_info.fault_next_thread = NULL;
	thread->vmm_info.swaptbl_next = NULL;
	thread->vmm_info.fault_entry = tbl->pages[PM_LINEAR_TO_TAB(pg_laddr)].entry.record;

	thread->vmm_info.page_in_address = vmm_get_page(task->id, (UINT32)pg_laddr);

	/* Set IO Lock on page */
	vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(vmm_get_physical(task->id, pg_laddr)), TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

	thread->swp_io_finished.callback = swap_page_read_callback;
	io_begin_pg_read( (pdir->tables[PM_LINEAR_TO_DIR(pg_laddr)].record.addr << 3), thread->vmm_info.page_in_address, thread);

	return TRUE;
}


/* 
Frees all pages on swap file for a given task. (ASYNCH) 
task->vmm_info.swap_free_addr must be set to 0 on the first call to this function.
*/
void vmm_swap_empty(struct pm_task *task, BOOL iocall)
{
	UINT16 creator_task_id = -1;
	UINT16 response_port = -1;
	UINT16 req_id = -1;

	/* Check directory entries looking for swapped tables */
	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;

	UINT32 i = ((UINT32)task->vmm_inf.swap_free_addr) / 0x400000;

	while((UINT32)task->vmm_inf.swap_free_addr < PMAN_MAPPING_BASE && task->vmm_inf.swap_page_count > 0)
	{
		if( pdir->tables[i].record.present == 0 && pdir->tables[i].record.swapped == 1 )
		{
			ADDR tbl_page = vmm_get_tblpage(task->id, i * 0x100000);

			task->vmm_inf.table_swap_addr = pdir->tables[i].record.addr;

			/* 
			Table is swapped, get a page for it and load from swap.
			*/
			pm_page_in(task->id, (ADDR)(i * 0x100000), (ADDR)LINEAR2PHYSICAL(tbl_page), 1, PGATT_WRITE_ENA);

			task->swp_io_finished.callback = swap_free_table_callback;
			io_begin_task_pg_read( (pdir->tables[i].record.addr << 3), tbl_page, task);

			task->vmm_inf.swap_page_count--;

			return;	// this function will continue when atac read messages are read.
		}
		else if( pdir->tables[i].ia32entry.present == 1 )
		{
			/* Check table entries. */
			struct vmm_page_table *pgtbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[i].b));
			UINT32 j = 0;

			for(j = 0; j < 1024; j++)
			{
				if(pgtbl->pages[j].entry.ia32entry.present == 0 && pgtbl->pages[j].entry.record.swapped == 1)
				{
					swap_free_addr( pgtbl->pages[j].entry.record.addr );
				}
			}

			/* If we came from a page table Swap IO, free the page paged in for this operation */
			if(iocall)
			{
				page_out(task->id, (ADDR)(i * 0x100000), 1);
				vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[i].b)));
			}
		}

		i++;
		task->vmm_inf.swap_free_addr = (ADDR)((UINT32)task->vmm_inf.swap_free_addr + 0x400000);
	}

	/*
	This is awful but I'll go through all IOSLOTS 
	looking for an ioslot with our task, and invalidate 
	them if found.
	*/
	for(i = 0; i < PMAN_IOSLOTS;i++)
	{
		if(ioslots.slots[i].type != IOSLOT_TYPE_FREE && ioslots.slots[i].owner_type == IOSLOT_OWNER_TYPE_TASK && ioslots.slots[i].task_id == task->id)
		{
			ioslots.slots[i].type = IOSLOT_TYPE_INVALIDATED;
		}
	}

	/* Begin File Mappings closing */
	vmm_fmap_close_all(task);
}

/* 
Invoked when finished bringing a page table from swap, when destroying a task. 
(remember to free the swap address used for this page table)
*/
UINT32 swap_free_table_callback(struct pm_task *task, UINT32 ioret)
{
	claim_mem(task->vmm_inf.swap_read_smo);
	task->vmm_inf.swap_read_smo = -1;

	/* Free Page table swap address */
	swap_free_addr( task->vmm_inf.table_swap_addr );

	/* Continue empy procedure */
	vmm_swap_empty(task, TRUE);

	return 1;
}

/*
A page has been brought from swap file
*/
UINT32 swap_page_read_callback(struct pm_thread *thread, UINT32 ioret)
{
	UINT32 perms = 0;
	struct pm_thread *curr_thr = NULL;
    struct pm_thread *othr = NULL;
	struct pm_task *task = NULL;

    // task can be killed, but it cannot be NULL, because there where swap operations pending

	if(ioret != IO_RET_ERR)
	{
		/* Got a successfull response for a read command */		
		task = tsk_get(thread->task_id);
        
		/* 
			This read command might have raised because 
			a table was being read
		*/
		if(thread->flags & THR_FLAG_PAGEFAULT)
		{
			if(task->state != TSK_KILLED)
				perms = swap_get_perms(task, (ADDR)PG_ADDRESS(thread->vmm_info.fault_address));

			/* Set swap address for the page as free (before page in) */
			swap_free_addr( thread->vmm_info.fault_entry.addr );
		
			/* Page has been fetched from swap */
			claim_mem(thread->vmm_info.fault_smo);
			thread->vmm_info.fault_smo = -1;

            /* Return page to our POOL if the task was killed */
            if(task->state == TSK_KILLED)
                vmm_put_page(thread->vmm_info.page_in_address);

			while(thread && thread->state == THR_KILLED)
			{
				/* Thread was killed! */
                thread->flags &= ~THR_FLAG_PAGEFAULT;
				
				/* Our thread was killed, see if there's other thread waiting for the same page */
				if(thread->vmm_info.fault_next_thread != NULL)
				{
					thread->vmm_info.fault_next_thread->vmm_info.page_in_address = thread->vmm_info.page_in_address;
                    othr = thread;
					thread = thread->vmm_info.fault_next_thread;
                    thr_destroy_thread(othr->id);
				}
				else 
				{
					if(task != NULL && task->state != TSK_KILLED)
                         vmm_put_page(thread->vmm_info.page_in_address);
                    thr_destroy_thread(thread->id);
                    return 1;
				}
			}
            
            if(task->state == TSK_KILLED && task->killed_threads == 0)
            {
                tsk_destroy(task);
                return 1;
            }

            if(thread == NULL) // al threads waiting for this page where killed
                return 1;

			/* Page in */
			pm_page_in(task->id, thread->vmm_info.fault_address, (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, perms);

			/* Remove IO lock from page */
			vmm_set_flags(task->id, (ADDR)PG_ADDRESS(thread->vmm_info.fault_address), TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

			/* Unmap page from pman address space */
			vmm_unmap_page(task->id, PG_ADDRESS(thread->vmm_info.fault_address));
			
			thread->flags &= ~THR_FLAG_PAGEFAULT;
			
			task->vmm_inf.page_count++;
			task->vmm_inf.swap_page_count--;
                        
			/* Wake all pending threads */
			vmm_wake_pf_threads(thread);
		}
		return 1;
	}
	else
	{
		vmm_put_page(thread->vmm_info.page_in_address);
                
        // remove page fault flag from threads so they are freed
        curr_thr = thread->vmm_info.fault_next_thread;

        while(curr_thr != NULL)
        {
            curr_thr->flags &= ~THR_FLAG_PAGEFAULT;
            curr_thr = curr_thr->vmm_info.fault_next_thread;
        }

        /* IO Error, raise an exception. (Terminate the task with error) */
		fatal_exception(thread->task_id, PG_IO_ERROR);
	}
	return 0;
}

/*
A page table has been brought from swap file
*/
UINT32 swap_table_read_callback(struct pm_thread *thread, UINT32 ioret)
{
	struct pm_thread *curr_thr, *bcurr, *othr;
    struct vmm_page_directory* pdir;
	struct pm_task *task = tsk_get(thread->task_id);

	if(ioret != IO_RET_ERR)
	{
		/* 
			This read command might have raised because 
			a table was being read
		*/
		if(thread->flags & THR_FLAG_PAGEFAULT_TBL)
		{
			/* Successfull Page lvl 1 (table) read */

			/* Set swap address for the page as free (before page in) */
            pdir = task->vmm_inf.page_directory;

			swap_free_addr( pdir->tables[PM_LINEAR_TO_DIR(thread->vmm_info.fault_address)].record.addr );
            
            claim_mem(thread->vmm_info.fault_smo);
			thread->vmm_info.fault_smo = -1;

            while(thread && thread->state == THR_KILLED)
			{
				/* Thread was killed! */
                thread->flags &= ~THR_FLAG_PAGEFAULT_TBL;
				
				/* Our thread was killed, see if there's other thread waiting for the same page */
				if(thread->vmm_info.fault_next_thread != NULL)
				{
					thread->vmm_info.fault_next_thread->vmm_info.page_in_address = thread->vmm_info.page_in_address;
                    othr = thread;
					thread = thread->vmm_info.fault_next_thread;
                    thr_destroy_thread(othr->id);
				}
				else 
				{					
				    if(task != NULL && task->state != TSK_KILLED)
                         vmm_put_page(thread->vmm_info.page_in_address);
                    thr_destroy_thread(thread->id);
                    return 1;
				}
			}
            
            if(task->state == TSK_KILLED && task->killed_threads == 0)
            {
                tsk_destroy(task);
                return 1;
            }

            if(thread == NULL) // al threads waiting for this page where killed
                return 1;
            
			/* 
			Page in, and process each pending Page Fault for this page table.
			This will be performed by going through the thread list for page tables.
			*/
			pm_page_in(task->id, thread->vmm_info.fault_address, (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 1, PGATT_WRITE_ENA);

			thread->flags &= ~THR_FLAG_PAGEFAULT_TBL;

			/* Process Page Faults for threads waiting for this page table */
			curr_thr = thread->vmm_info.swaptbl_next;

			task->vmm_inf.page_count++;
			task->vmm_inf.swap_page_count--;
			
			while(curr_thr != NULL)
			{
				curr_thr->vmm_info.fault_smo = -1;
				curr_thr->flags &= ~THR_FLAG_PAGEFAULT_TBL;

                /* 
					If thread was not waiting for the same page, process 
					it's page fault. 
				*/
				if(PG_ADDRESS(thread->vmm_info.fault_address) != PG_ADDRESS(curr_thr->vmm_info.fault_address))
				{
					bcurr = curr_thr->vmm_info.swaptbl_next;

                    while(curr_thr && curr_thr->state == THR_KILLED)
                    {
                        curr_thr->flags &= ~THR_FLAG_PAGEFAULT_TBL;

                        // remove the threads killed
                        othr = curr_thr;
                        curr_thr = thread->vmm_info.fault_next_thread;
                        thr_destroy_thread(othr->id);
                    }
                    
					if(curr_thr && !vmm_handle_page_fault(&curr_thr->id, TRUE))
					{
						/* Wake all pending threads (same table, same page) */
						vmm_wake_pf_threads(curr_thr);
					}
				}
                
				curr_thr = bcurr;
			}

			/* Process our page fault */
			if(vmm_handle_page_fault(&thread->id, TRUE)) 
				return 1;

			/* Wake all pending threads */
			vmm_wake_pf_threads(thread);

			return 1;
		}
		return 0;
	}
    else
    {
        vmm_put_page(thread->vmm_info.page_in_address);
                
        // remove page fault flag from threads so they are freed
        curr_thr = thread->vmm_info.fault_next_thread;

        while(curr_thr != NULL)
        {
            curr_thr->flags &= ~THR_FLAG_PAGEFAULT;
            curr_thr = curr_thr->vmm_info.fault_next_thread;
        }

        if(task != NULL)
        {
		    /* IO Error, raise an exception. (Terminate the task with error) */
		    fatal_exception(thread->task_id, PG_IO_ERROR);
        }
    }
	return 0;
}

/* Initialize Swap related stuff */
void vmm_swap_init()
{
	struct pm_thread *pmthr;
    	
	/* 
	Using available mem calculate mem thresholds.
	TODO: Set this numbers to a more accurate value.
	*/
	if(vmm.available_mem < 0x2000000) 
	{
		/* Memory available is below 32MB */
		vmm.max_mem = vmm.available_mem - (UINT32)(vmm.available_mem / 4) * 3;
		vmm.mid_mem = vmm.available_mem - (UINT32)(vmm.available_mem / 4) * 2;
		vmm.min_mem = vmm.available_mem - (UINT32)(vmm.available_mem / 8);
	}
	else
	{
		/* Memory available is above 32MB */
		vmm.max_mem = (UINT32)(vmm.available_mem / 4);
		vmm.mid_mem = (UINT32)(vmm.available_mem / 7);
		vmm.min_mem = (UINT32)(vmm.available_mem / 10);
	}

	/* Set current regions */
	curr_st_region = 0;
	curr_ag_region = (UINT32)(vmm.available_mem / 0x200000);	// half the available memory
	
	vmm.swap_thr_distance = (UINT32)(vmm.available_mem / 0x200000);
	vmm.swap_start_index = PM_LINEAR_TO_DIR(FIRST_PAGE(PMAN_POOL_LINEAR));
	vmm.swap_end_index = vmm.vmm_tables;

	pman_print("SWP: Params Initialized. Mem: %i, MaxM: %i MB, MidM: %i MB, MinM: %i MB ", vmm.available_mem / 0x100000, vmm.max_mem / 0x100000, vmm.mid_mem / 0x100000, vmm.min_mem / 0x100000);
}

void vmm_swap_found(UINT32 partition_size, UINT32 ldevid)
{
	// REMOVE ME FOR SWAP TESTING PLEASE!! //
	vmm.swap_present = FALSE;
	return;
	/////////////////////////////////////////

	struct thread thr;
	struct pm_thread *pmthr;

	vmm.swap_present = TRUE;
	vmm.swap_ldevid = ldevid;
	vmm.swap_available = vmm.swap_size = (partition_size >> 3); // Divide by 8 (8 disk blocks = 1 page)
	
	/* initialize swap bitmap */
	init_swap_bitmap();

	/* Create threads */
	thr.task_num = PMAN_TASK;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 3;
	thr.ep = &vmm_page_aging;
	thr.stack = (void *)STACK_ADDR(PMAN_AGING_STACK_ADDR);

	if(create_thread(PAGEAGING_THR, &thr) < 0)
	{
		pman_print_and_stop("Could not create page aging thread.");
		STOP;
	}

	thr.task_num = PMAN_TASK;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 3;
	thr.ep = &vmm_page_stealer;
	thr.stack = (void *)STACK_ADDR(PMAN_STEALING_STACK_ADDR);

	if(create_thread(PAGESTEALING_THR, &thr) < 0)
	{
		pman_print_and_stop("Could not create page stealing thread.");
		STOP;
	}

	pmthr = thr_create(PAGEAGING_THR, NULL);

	pmthr->state = THR_INTERNAL;
	pmthr->sch.priority = 2;
		
	/* Activate Virtual Memory Threads. */
	sch_add(pmthr);
	sch_activate(pmthr);
	
	pmthr = thr_create(PAGESTEALING_THR, NULL);
        
	pmthr->state = THR_INTERNAL;
	pmthr->sch.priority = 2;

	sch_add(pmthr);
	sch_activate(pmthr);
}

