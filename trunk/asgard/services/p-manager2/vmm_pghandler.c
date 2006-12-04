
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "layout.h"
#include "vm.h"
#include "types.h"
#include "exception.h"
#include "scheduler.h"
#include "loader.h"
#include "task_thread.h"
#include "formats/ia32paging.h"

/*
thr_id: If not NULL it will be set to the page fault rising thread id. (If its an internal call this MUST contain the thread id)
internal: If TRUE, handler was invoked by pman, FALSE if invoked by PF interrupt.
*/
BOOL vmm_handle_page_fault(UINT16 *thr_id, BOOL internal)
{
	struct page_fault pf;
	ADDR page_addr;
	UINT16 task_id, thread_id;
	struct pm_thread *curr_thr = NULL;
	struct pm_thread *thread = NULL;
	struct pm_task *task = NULL;
	struct vmm_page_table *optbl = NULL;
	struct vmm_page_directory *directory = NULL;
	UINT32 filepos, readsize;
	INT32 perms, page_displacement;

	if(!internal)
	{
		/* Fault was generated by PF interrupt */
		get_page_fault(&pf);

		if(pf.task_id == PMAN_TASK)
			pman_print_and_stop("PMAN: INTERNAL PF linear: %x, task: %i, thr: %i ", pf.linear, pf.task_id, pf.thread_id);
		
		task_id = pf.task_id; 
		thread_id = pf.thread_id;
		
		thread = thr_get(thread_id);
		task = tsk_get(task_id);
		
		if(thread->state == THR_INTHNDL)
			pman_print_and_stop("INT HADLER PAGE FAULT!");

		if(thread == NULL || task == NULL)
			pman_print_and_stop("PF: NULL TASK/THREAD");

		//if(!(task->flags & TSK_FLAG_SYS_SERVICE)) 
		//	pman_print("PMAN: PROC PF linear: %x, task: %i, thr: %i ", pf.linear, pf.task_id, pf.thread_id);

		thread->vmm_info.fault_entry.addr = 0;
		thread->vmm_info.fault_entry.present = 0;
		thread->vmm_info.fault_entry.swapped = 0;
		thread->vmm_info.fault_entry.unused = 0;

		if(thr_id != NULL) *thr_id = thread_id;

		/* Check Page is not being fetched by other thread of the same task */
		curr_thr = task->first_thread;

		while(curr_thr != NULL)
		{
			if( curr_thr != thread && thread->vmm_info.fault_next_thread == NULL && (curr_thr->flags & THR_FLAG_PAGEFAULT) && PG_ADDRESS(curr_thr->vmm_info.fault_address) == PG_ADDRESS(pf.linear) )
			{
				/* found last thread waiting for this page */
				curr_thr->vmm_info.fault_next_thread = thread;	// set next thread as our current thread
				break;
			}

			curr_thr = curr_thr->next_thread;
		}

		if(curr_thr != NULL)
		{			
			sch_deactivate(thread);

			/* Page was being retrieved already for other thread! Block the thread.	*/
			thread->state = THR_BLOCKED;
			thread->flags |= THR_FLAG_PAGEFAULT;
			thread->vmm_info.fault_address = pf.linear;
			thread->vmm_info.read_size = curr_thr->vmm_info.read_size;
			thread->vmm_info.page_perms = curr_thr->vmm_info.page_perms;
			thread->vmm_info.page_displacement = curr_thr->vmm_info.page_displacement;
			thread->vmm_info.fault_next_thread = NULL;
			thread->vmm_info.swaptbl_next = NULL;

			return TRUE;	// thread will remain on hold
		}		
	}
	else
	{
		pman_print_and_stop("PF: INTERNAL FAULT");
		/* 
		Internal faults will be generated by the process manager itself when a 
		page table is fetched from swap file. 
		*/
		thread = thr_get(*thr_id);

		if(thread->flags & THR_FLAG_PAGEFAULT)
		{
			/* Thread already is being handled, keep it blocked. */
			return TRUE;
		}

		task_id = thread->task_id; 
		thread_id = *thr_id;

		task = tsk_get(task_id);

		thread->vmm_info.fault_entry.addr = 0;
		thread->vmm_info.fault_entry.present = 0;
		thread->vmm_info.fault_entry.swapped = 0;
		thread->vmm_info.fault_entry.unused = 0;

		/* Build a bogus page fault */
		pf.linear = thread->vmm_info.fault_address;
		pf.task_id = task_id;
		pf.thread_id = thread_id;
	}
	
	thread->vmm_info.fault_address = pf.linear;

	/* Check PF is not above max_addr */
	if(task->vmm_inf.max_addr <= (UINT32)pf.linear)
	{
		thread->state = THR_EXEPTION;
		sch_deactivate(thread);

		// FIXME: Should send an exception signal... 
		return TRUE;
	}

	/* Check if page table is on swap */
	/*if(!(task->flags & TSK_FLAG_SYS_SERVICE) && vmm_check_swap_tbl(task, thread, (ADDR)PG_ADDRESS(pf.linear)))
	{
		return TRUE;
	}*/

	/* Lets see if the page table is present on the page directory and if not give it one */
	if(task->vmm_inf.page_directory->tables[PM_LINEAR_TO_DIR(pf.linear)].ia32entry.present == 0)
	{
		/* Get a Page and set taken */
		page_addr = vmm_get_tblpage(task->id, PG_ADDRESS(pf.linear));
		
		/* Page in the table on task linear space. */
		pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 1, PGATT_WRITE_ENA);

		task->vmm_inf.page_count++;
	}
	
	/* See if page is file mapped */
	/*if(vmm_page_filemapped(task, thread, (ADDR)PG_ADDRESS(pf.linear)))
	{
		// Page is file mapped and a read operation has begun. //
		return TRUE;
	}*/

	/* 
	If it's a shared page, see if it's paged in on the owner task.
	*/
	/*
	if(vmm_is_shared(task, (ADDR)PG_ADDRESS(pf.linear))) // REMOVE 0
	{
		ADDR owner_laddr = NULL;
		UINT32 attrib = 0;

		// Page is shared, check if it's present on the owner task. //
		struct pm_task *otsk = vmm_shared_getowner(task, (ADDR)PG_ADDRESS(pf.linear), &owner_laddr, &attrib);

		optbl = (struct vmm_page_table *)PHYSICAL2LINEAR(otsk->vmm_inf.page_directory->tables[PM_LINEAR_TO_DIR(owner_laddr)].b);
		
		if(optbl->pages[PM_LINEAR_TO_TAB(owner_laddr)].entry.ia32entry.present == 0)
		{
			// We won't page out shared pages... if this happened we are screwed. //
		}
		else
		{
			// Map the page //
			pm_page_in(task->id, (ADDR)pf.linear, (ADDR)PG_ADDRESS(optbl->pages[PM_LINEAR_TO_TAB(owner_laddr)].entry.phy_page_addr), 2, attrib);
			return FALSE;
		}
	}
	*/

	/* If task is not a system service, check if page requested is on Swap. */
	/*if(!(task->flags & TSK_FLAG_SYS_SERVICE) && vmm_check_swap(task, thread, (ADDR)PG_ADDRESS(pf.linear)) )
	{
		return TRUE;
	}*/
		
	directory = task->vmm_inf.page_directory;

	/* Check if Page belongs to the executable file or is just data */
	filepos = 0;
	readsize = 0;
	page_displacement = 0;
	perms = PGATT_WRITE_ENA;

	/* Must page be read from elf file? (initial system services will be pre loaded) */
	if(!(task->flags & TSK_FLAG_SYS_SERVICE) && loader_filepos(task, pf.linear, &filepos, &readsize, &perms, &page_displacement))
	{		
		/* Thread must be blocked until data is read from the executable file */
		sch_deactivate(thread);

		thread->state = THR_BLOCKED;
		thread->flags |= THR_FLAG_PAGEFAULT;
		thread->vmm_info.fault_address = pf.linear;
		thread->vmm_info.read_size = readsize;
		thread->vmm_info.page_perms = perms;
		thread->vmm_info.page_displacement = page_displacement;
		thread->vmm_info.fault_next_thread = NULL;
		thread->vmm_info.swaptbl_next = NULL;

		/* 
		Page will be granted before hand, so page stealing thread won't attempt to take our page table.
		We also set taken entry here.
		*/
		thread->vmm_info.page_in_address = vmm_get_page(task->id, PG_ADDRESS(pf.linear));

		task->vmm_inf.page_count++;

		/* IO lock page table */
		vmm_set_flags(task_id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(task->id, pf.linear)), TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

		/* IO Lock Page */
		vmm_set_flags(task_id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

		thread->io_finished.callback = vmm_elffile_seekend_callback;
		
		io_begin_seek(&thread->io_event_src, filepos);

		if(thread->vmm_info.swaptbl_next == NULL)
		{
			/* 
			Check if page fault is on a table currently being used by 
			other page fault IO. (this prevents removing IO lock from
			a table while pfaults are still being served) 
			*/

			/* Set vmm_info.swaptbl_next to the next thread waiting for a page on this table */
			struct pm_thread *curr_thr = task->first_thread;

			while(curr_thr != NULL)
			{
				if( curr_thr != thread && (thread->flags & THR_FLAG_PAGEFAULT) && thread->vmm_info.swaptbl_next == NULL && PM_LINEAR_TO_DIR(thread->vmm_info.fault_address) == PM_LINEAR_TO_DIR(PG_ADDRESS(pf.linear)) )
				{
					/* found last thread waiting for this page table */
					curr_thr->vmm_info.swaptbl_next = thread;	// set next thread as our current thread
					break;
				}

				curr_thr = curr_thr->next_thread;
			}
		}

		return TRUE;
	} 
	else 
	{		
		/*
		Get a page and set taken.
		*/
		page_addr = vmm_get_page(task->id, (UINT32)pf.linear);

		/*
		Map page onto the process address space.
		*/
		if(pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 2, perms) != SUCCESS)
			pman_print_and_stop("Not good... ");
			
		/*
		Unmap page from PMAN linear address space. 
		*/
		vmm_unmap_page(task->id, (UINT32)PG_ADDRESS(pf.linear));
		
		task->vmm_inf.page_count++;

		return FALSE;
	}	
}

/*
Page Fault Interrupt Handler
*/
void vmm_paging_interrupt_handler()
{
	for(;;)
	{
		UINT16 thread_id;

		if(vmm_handle_page_fault(&thread_id, 0) || sch_running() == 0xFFFF) 
		{
			/* Resume scheduler for thread was put on hold */
			run_thread(SCHED_THR);
		}
		else
		{
			/* Resume currently running thread */
			run_thread(sch_running());
		}
	}
}

/* 
This function will be invoked upon completion of a seek command 
issued by the page fault handler.
*/
INT32 vmm_elffile_seekend_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_thread *thread, *curr_thr;
	struct pm_task *task;

	thread = thr_get(iosrc->id);

	if(ioret != IO_RET_OK)
	{
		/* Return page to vmm, for it hasn't been mapped yet and it wouldn't be freed. */
		vmm_put_page(thread->vmm_info.page_in_address);
		thread->vmm_info.page_in_address = NULL;

		/* Finish with a fatal exception. */
		fatal_exception(thread->task_id, PG_IO_ERROR);
		return 0;
	}	

	/* Check for killed thread */
	if(thread->state == THR_KILLED)
	{
		task = tsk_get(thread->task_id);

		/* Thread was killed! */
		if(task->state == TSK_KILLED)
		{	
			/* Task is also on death line */
			
			/* If there are other threads waiting for our page, kill them */
			if(thread->vmm_info.fault_next_thread != NULL)
			{
				curr_thr = thread->vmm_info.fault_next_thread;
				
				while(curr_thr != NULL)
				{
					if(curr_thr->state == THR_KILLED)
						thr_destroy_thread(curr_thr->id);

					curr_thr = curr_thr->vmm_info.fault_next_thread;							
				}						
			}

			thr_destroy_thread(thread->id);

			if(task->killed_threads == 0)
				tsk_destroy(task);	

			return 1;
		}
		else
		{
			thr_destroy_thread(thread->id);

			/* Only our thread was killed, see if there's other thread waiting for the same page */
			if(thread->vmm_info.fault_next_thread != NULL)
			{
				thread->vmm_info.fault_next_thread->vmm_info.page_in_address = thread->vmm_info.page_in_address;
				thread = thread->vmm_info.fault_next_thread;
			}
			else 
			{
				return 1;
			}
		}
	}


	/* Read from File */
	thread->io_finished.callback = vmm_elffile_readend_callback;
	io_begin_read(iosrc, thread->vmm_info.read_size, (ADDR)((UINT32)thread->vmm_info.page_in_address + thread->vmm_info.page_displacement));

	return 1;
}

/*
Function invoked upong completion of a read command from the executable file.
*/
INT32 vmm_elffile_readend_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_thread *thread, *curr_thr;
	struct pm_task *task;

	thread = thr_get(iosrc->id);

	if(ioret != IO_RET_OK)
	{
		/* Return page to vmm, for it hasn't been mapped yet and it wouldn't be freed. */
		vmm_put_page(thread->vmm_info.page_in_address);

		/* Finish with a fatal exception. */
		fatal_exception(thread->task_id, PG_IO_ERROR);
		return 0;
	}

	/* Check for thread killed */
	if(thread->state == THR_KILLED)
	{
		/* Thread was killed! */
		if(task->state == TSK_KILLED)
		{	
			/* Task is also on death line */

			/* Return page to our POOL */
			vmm_put_page(thread->vmm_info.page_in_address);

			/* If there are other threads waiting for our page, kill them */
			if(thread->vmm_info.fault_next_thread != NULL)
			{
				curr_thr = thread->vmm_info.fault_next_thread;
				
				while(curr_thr != NULL)
				{
					if(curr_thr->state == THR_KILLED)
						thr_destroy_thread(curr_thr->id);

					curr_thr = curr_thr->vmm_info.fault_next_thread;							
				}						
			}

			thr_destroy_thread(thread->id);

			if(task->killed_threads == 0)
				tsk_destroy(task);	

			return 1;
		}
		else
		{
			thr_destroy_thread(thread->id);

			/* Only our thread was killed, see if there's other thread waiting for the same page */
			if(thread->vmm_info.fault_next_thread != NULL)
			{
				thread->vmm_info.fault_next_thread->vmm_info.page_in_address = thread->vmm_info.page_in_address;
				thread = thread->vmm_info.fault_next_thread;
			}
			else 
			{
				vmm_put_page(thread->vmm_info.page_in_address);
				return 1;
			}
		}
	}
	
	/* Page in on process address space */
	pm_page_in(thread->task_id, (ADDR)PG_ADDRESS(thread->vmm_info.fault_address), (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);

	/* Un set IOLCK eflags on the page */
	vmm_set_flags(thread->task_id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
	
	/* Remove page from pman address space and create assigned record. */
	vmm_unmap_page(thread->task_id, PG_ADDRESS(thread->vmm_info.fault_address));

	/* Wake threads waiting for this page */
	vmm_wake_pf_threads(thread);

	return 1;
}

/*
Re-enable Threads Waiting for the same page.
*/
void vmm_wake_pf_threads(struct pm_thread *thread)
{
	struct pm_thread *curr_thr, *old_curr;

	/* First things first: Unblock our thread and reactivate it. */
	thread->state = THR_WAITING;	
	thread->flags &= ~THR_FLAG_PAGEFAULT;

	sch_activate(thread);

	/* 
	Go through task threads and enable all threads waiting for the same page.
	(pm_page_in is not necesary for the task is the same) 
	*/
	curr_thr = thread->vmm_info.fault_next_thread;
	old_curr = thread;

	while(curr_thr != NULL)
	{
		if(curr_thr->state != THR_KILLED)
		{
			curr_thr->state = THR_WAITING;	
			curr_thr->flags &= ~THR_FLAG_PAGEFAULT;

			sch_activate(curr_thr);
		}

		curr_thr = curr_thr->vmm_info.fault_next_thread;
		old_curr->vmm_info.fault_next_thread = NULL;
		old_curr = curr_thr;
	}

	/* 
		If there are no threads on the vmm_info.swaptbl_next list unlock the table.
	*/

	/* Find predecesor on tbl list */		
	curr_thr = thread->vmm_info.swaptbl_next;
	
	while(curr_thr != NULL)
	{
		if(curr_thr->vmm_info.swaptbl_next == thread) break;
		curr_thr = curr_thr->vmm_info.swaptbl_next;
	}
	
	if(thread->vmm_info.swaptbl_next == NULL && curr_thr == NULL)
	{
		/* Un set IOLCK on the table */
		vmm_set_flags(thread->task_id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(thread->task_id, thread->vmm_info.fault_address)), TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
	}
	else if(curr_thr != NULL)
	{
		/* Fix table list. */
		curr_thr->vmm_info.swaptbl_next = thread->vmm_info.swaptbl_next;
	}
	thread->vmm_info.swaptbl_next = NULL;
}
