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
#include "vm.h"
#include "types.h"
#include "exception.h"
#include "scheduler.h"
#include "loader.h"
#include "task_thread.h"
#include "formats/ia32paging.h"
#include "rb.h"

/*
thr_id: If not NULL it will be set to the page fault rising thread id. (If its an internal call this MUST contain the thread id)
internal: If TRUE, handler was invoked by pman, FALSE if invoked by PF interrupt.
*/
BOOL vmm_handle_page_fault(UINT16 *thr_id, BOOL internal)
{
	struct page_fault pf;
	ADDR page_addr;
	UINT16 task_id, thread_id;
	struct pm_thread *thread = NULL;
	struct pm_task *task = NULL;
	struct vmm_page_directory *directory = NULL;
	UINT32 filepos, readsize;
	INT32 perms, page_displacement;
    unsigned int error_code = 0;
    
	if(!internal)
	{
        /* Fault was generated by PF interrupt */
		get_page_fault(&pf);
        get_last_int(&error_code);
        
        /* is sartoris requesting/returning memory? */
        if(pf.flags != PF_FLAG_NONE && pf.flags != PF_FLAG_EXT)
        {
            if(pf.flags == PF_FLAG_FREE)
            {
                // sartoris is returning a page
                // last_page_fault.linear contains the physical address
                vmm_pm_put_page((ADDR)PHYSICAL2LINEAR(pf.linear));
            }
            else
            {
                thread = thr_get(pf.thread_id);

                // sartoris is requesting pages
                page_addr = vmm_pm_get_page(FALSE);

                if(page_addr == NULL)
                    pman_print_dbg("PMAN: NO FREE PAGE FOR SARTORIS");

                if(grant_page_mk(LINEAR2PHYSICAL(page_addr)) < 0)
                    pman_print_and_stop("PMAN: grant_page_mk failed!");
            }
            // don't put the thread on hold.
            return FALSE;
        }
        else
        {
		    if(pf.task_id == PMAN_TASK)
			    pman_print_and_stop("PMAN: INTERNAL PF linear: %x, task: %i, thr: %i ", pf.linear, pf.task_id, pf.thread_id);

		    task_id = pf.task_id; 
		    thread_id = pf.thread_id;
		
		    thread = thr_get(thread_id);
		    task = tsk_get(task_id);

		    if(thread == NULL || task == NULL)
			    pman_print_and_stop("PF: NULL TASK/THREAD");

		    if(thread->state == THR_INTHNDL)
            {
			    pman_print_and_stop("INT HADLER PAGE FAULT! linear %x, task: %i, thr: %i ", pf.linear, pf.task_id, pf.thread_id);
            }
        		
		    thread->vmm_info.fault_entry.addr = 0;
		    thread->vmm_info.fault_entry.present = 0;
		    thread->vmm_info.fault_entry.swapped = 0;
		    thread->vmm_info.fault_entry.unused = 0;
            thread->vmm_info.fault_task = task_id;

		    if(thr_id != NULL) *thr_id = thread_id;

            /*
            Check it's not a page fault raised because of rights
            */
            if(error_code & PF_ERRORCODE_PROTECTION)
            {
                // page is present!, but either on the table or the page the RW flag
                // is 0. This means we are dealing with a lack of privileges.
                fatal_exception(thread->task_id, PG_RW_ERROR);

                return TRUE;
            }

		    /* Check Page is not being fetched by other thread */
            thread->vmm_info.pg_node.value = PG_ADDRESS(pf.linear);
                        
            if(rb_insert(&task->vmm_info.wait_root, &thread->vmm_info.pg_node, TRUE) == 2)
            {
                sch_deactivate(thread);

                struct pm_thread *thr = thr_get(thread->vmm_info.pg_node.prev->value2);

			    /* Page was being retrieved already for other thread! Block the thread.	*/
			    thread->state = THR_BLOCKED;
			    thread->flags |= THR_FLAG_PAGEFAULT;
			    thread->vmm_info.fault_address = pf.linear;
			    thread->vmm_info.read_size =  0;
			    thread->vmm_info.page_perms = 0;
			    thread->vmm_info.page_displacement = 0;
			    thread->vmm_info.fault_task = task_id;

			    return TRUE;	// thread will remain on hold
		    }
        }
	}
	else
	{
		/* 
		Internal faults will be generated by the process manager itself when a 
		page table is fetched from swap file. 
        NOTE: If there was another thread waiting for the same page, this thread
        must already be on the tree, and will be handled when the page is fetched
        for that thread.
		*/
		thread = thr_get(*thr_id);

		if(thread->flags & THR_FLAG_PAGEFAULT)
		{
			/* Thread already is being handled, keep it blocked. */
			return TRUE;
		}

		task_id = thread->vmm_info.fault_task; 
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
    	    
	/* Check if page table is on swap.
    NOTE: This can only happen when all pages had been sent to swap. The page table
    was also sent to swap because we cannot discard it, for it has swap addresses for it's pages.
    */
	if(!(task->flags & TSK_FLAG_SYS_SERVICE) 
        && vmm_check_swap_tbl(task, thread, (ADDR)PG_ADDRESS(pf.linear)))
	{
        pman_print_dbg("PF: Table is on SWAP \n");
        // we must insert the thread on the pg wait tree
        // in case another thread asks for the same page.
        return TRUE;
	}
    
    /*
    Does this page collide with a memory region?
    */
    if(task->vmm_info.regions != NULL)
    {
        ma_node *n = ma_search_point(&task->vmm_info.regions, (UINT32)pf.linear);
        int libret;

	    if(n != NULL)
        {
            /* Lets see if the page table is present on the page directory and if not give it one */
	        if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(pf.linear)].ia32entry.present == 0)
	        {
		        /* Get a Page and set taken */
		        page_addr = vmm_get_tblpage(task->id, PG_ADDRESS(pf.linear));
		
		        /* Page in the table on task linear space. */
		        pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 1, PGATT_WRITE_ENA);

		        task->vmm_info.page_count++;
	        }

            struct vmm_memory_region *mreg = VMM_MEMREG_MEMA2MEMR(n);

            switch(mreg->type)
            {
                case VMM_MEMREGION_FMAP:
                    if(vmm_page_filemapped(task, thread, (ADDR)PG_ADDRESS(pf.linear), mreg))
                        return TRUE;                                                    // Page is file mapped and a read operation has begun.
                    break;
                case VMM_MEMREGION_SHARED:
                    return vmm_page_shared(task, (ADDR)PG_ADDRESS(pf.linear), mreg);    // page is shared
                case VMM_MEMREGION_LIB:
                    libret = vmm_page_lib(task, thread, (ADDR)PG_ADDRESS(pf.linear), mreg);

                    if(libret == 1) return FALSE;
                    else if(libret == 2) return TRUE;
                    else
                    {
                        page_addr = vmm_get_page(task->id, (UINT32)pf.linear);
 
                        pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 2, perms);
			                
		                vmm_unmap_page(task->id, (UINT32)PG_ADDRESS(pf.linear));

		                task->vmm_info.page_count++;

		                return FALSE;
                    }
                break;
            }
        }
    }

    /* 
    Check Page fault is on a valid memory area.
    */
    if((UINT32)pf.linear - SARTORIS_PROCBASE_LINEAR >= task->vmm_info.max_addr)
    {
        // ok it's above max addr.. is it a stack?


	    if((UINT32)pf.linear - SARTORIS_PROCBASE_LINEAR >= PMAN_MAPPING_BASE)
	    {
            pman_print_dbg("PF: to high %x\n", pf.linear);
		    fatal_exception(thread->task_id, MAXADDR_ERROR);
		    return TRUE;
	    }
    }

	/* Lets see if the page table is present on the page directory and if not give it one */
	if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(pf.linear)].ia32entry.present == 0)
	{
		/* Get a Page and set taken */
		page_addr = vmm_get_tblpage(task->id, PG_ADDRESS(pf.linear));
		
		/* Page in the table on task linear space. */
		pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 1, PGATT_WRITE_ENA);

		task->vmm_info.page_count++;
	}
    
	/* If task is not a system service, check if page requested is on Swap. */
	if(!(task->flags & TSK_FLAG_SYS_SERVICE) 
        && vmm_check_swap(task, thread, (ADDR)PG_ADDRESS(pf.linear)) )
	{
        pman_print_dbg("PF: Page is swapped \n");
        // we must insert the thread on the pg wait tree
        // in case another thread asks for the same page.
        return TRUE;
	}
	
	directory = task->vmm_info.page_directory;

	/* Check if Page belongs to the executable file or is just data */
	filepos = 0;
	readsize = 0;
	page_displacement = 0;
	perms = PGATT_WRITE_ENA;

	/* Must page be read from elf file? (initial system services will be pre loaded) */
	if(!(task->flags & TSK_FLAG_SYS_SERVICE) 
        && loader_filepos(task, pf.linear, &filepos, &readsize, &perms, &page_displacement, NULL))
	{		
        /* Thread must be blocked until data is read from the executable file */
		sch_deactivate(thread);

		thread->state = THR_BLOCKED;
		thread->flags |= THR_FLAG_PAGEFAULT;
		thread->vmm_info.fault_address = pf.linear;
		thread->vmm_info.read_size = readsize;
		thread->vmm_info.page_perms = perms;
		thread->vmm_info.page_displacement = page_displacement;
        thread->vmm_info.fault_task = task_id;
        thread->vmm_info.pg_node.value = PG_ADDRESS(pf.linear);

		/* 
		Page will be granted before hand, so page stealing thread won't attempt to take our page table.
		We also set taken entry here.
		*/
		thread->vmm_info.page_in_address = vmm_get_page(task->id, PG_ADDRESS(pf.linear));
        
		task->vmm_info.page_count++;
        
        /* IO Lock page */
        vmm_set_flags(task_id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

		thread->io_finished.callback = vmm_elffile_seekend_callback;

        // if it's a different task, set the IO source
        if(thread->task_id != task_id)
        {
            io_set_src(&thread->io_event_src, &task->io_event_src);
        }
		
		io_begin_seek(&thread->io_event_src, filepos);

        // we must insert the thread on the pg wait tree
        // in case another thread asks for the same page.
        rb_insert(&task->vmm_info.wait_root, &thread->vmm_info.pg_node, FALSE);

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
		pm_page_in(task->id, (ADDR)PG_ADDRESS(pf.linear), (ADDR)LINEAR2PHYSICAL(page_addr), 2, perms);
			
		/*
		Unmap page from PMAN linear address space. 
		*/
		vmm_unmap_page(task->id, (UINT32)PG_ADDRESS(pf.linear));

		task->vmm_info.page_count++;

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
	struct pm_thread *thread;
    
	thread = thr_get(iosrc->id);
    
	if(ioret != IO_RET_OK)
	{
        pman_print_dbg("PF: Seek End IOERR \n");
		vmm_page_ioerror(thread, FALSE);
		return 0;
	}

    vmm_check_threads_pg(&thread, FALSE);

    if(!thread)
        return 0;

	/* Read from File */
	thread->io_finished.callback = vmm_elffile_readend_callback;
	io_begin_read(iosrc, thread->vmm_info.read_size, (ADDR)((UINT32)thread->vmm_info.page_in_address + thread->vmm_info.page_displacement));

	return 1;
}

/*
Function invoked upon completion of a read command from the executable file.
*/
INT32 vmm_elffile_readend_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_thread *thread;
	struct pm_task *task;

	thread = thr_get(iosrc->id);

	if(ioret != IO_RET_OK)
	{
		vmm_page_ioerror(thread, FALSE);
		return 0;
	}

    vmm_check_threads_pg(&thread, FALSE);

    if(!thread)
        return 1;
    
    // if we are on a shared lib fetch, we should page_in the page on both
    // the thread and the lib task
    task = tsk_get(thread->vmm_info.fault_task);

    if(task->flags & TSK_SHARED_LIB)
    {
        /* Page in on process address space */
        pm_page_in(thread->task_id, (ADDR)PG_ADDRESS(thread->vmm_info.fault_address), (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);
        
        // see if it's an executable section of the library
        struct taken_entry *tentry = vmm_taken_get(thread->vmm_info.page_in_address);

        if(tentry->data.b_pg.flags & TAKEN_PG_FLAG_LIBEXE)
        {
            /* Page in on library address space */
            pm_page_in(thread->vmm_info.fault_task, (ADDR)PG_ADDRESS(thread->vmm_info.pg_node.value), (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);

            /* Remove page from pman address space and create assigned record. */
            vmm_unmap_page(task->id, PG_ADDRESS(thread->vmm_info.pg_node.value));
        }
        else
        {
            /* Remove page from pman address space and create assigned record. */
            vmm_unmap_page(thread->task_id, PG_ADDRESS(thread->vmm_info.fault_address));
        }
    }
    else
    {
        /* Page in on process address space */
        pm_page_in(thread->vmm_info.fault_task, (ADDR)PG_ADDRESS(thread->vmm_info.fault_address), (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);

        vmm_unmap_page(thread->task_id, PG_ADDRESS(thread->vmm_info.fault_address));
    }
      
    /* Un set IOLCK on the page */
    vmm_set_flags(task->id, (ADDR)thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
    
    /* Wake threads waiting for this page */
    vmm_wake_pf_threads(thread);

	return 1;
}

void vmm_page_ioerror(struct pm_thread *thread, BOOL removeTBLTree)
{
    struct pm_thread *curr_thr;
    int task_id = thread->task_id;
    rbnode n = thread->vmm_info.pg_node;
    rbnode *currnode = NULL, *on = NULL;
    struct pm_task *task = tsk_get(thread->vmm_info.fault_task), *t;

    pman_print_dbg("PF: IO ERROR \n");

    /* Return page to vmm, for it hasn't been mapped yet and it wouldn't be freed. */
	if(thread->vmm_info.page_in_address)
    {
        vmm_put_page(thread->vmm_info.page_in_address);
        task->vmm_info.page_count--;
        thread->vmm_info.page_in_address = NULL;
    }
    
    currnode = thread->vmm_info.pg_node.next;
        
    // If there's a thread from other task waiting, we will:
    // - Send an exception if it was a library read.
    // - Continue pending threads if it's because of an smo
    //   operation (read/write will fail).
    // We won't do anything with the faulting task except it's 
    // the thread task.
    while(currnode != NULL)
    {
        on = currnode->next;
        
        curr_thr = thr_get(currnode->value2);

        if(curr_thr->task_id != task_id)
        {
            t = tsk_get(curr_thr->task_id);

            if(task->flags & TSK_SHARED_LIB)
            {
                // kill the task
                fatal_exception(t->id, PG_IO_ERROR);
            }
            else
            {
                curr_thr->flags &= ~(THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL);
                
                if(curr_thr->state == THR_KILLED)
                {
                    rb_remove_child(&task->vmm_info.wait_root, currnode);
                    if(removeTBLTree)
                        rb_remove_child(&task->vmm_info.tbl_wait_root, &thread->vmm_info.tbl_node);
                
                    thr_destroy_thread(thread->id);

                    tsk_destroy(task);
                }
                else
                {
                    // wake the thread
                    curr_thr->state = THR_WAITING;
			        sch_activate(curr_thr);
                }
            }
        }
        else
        {
            curr_thr->flags &= ~THR_FLAG_PAGEFAULT;
        }

        currnode = on;
    }

    // restore the IO source it it's not the same task
    if(thread->task_id != task->id)
    {
        t = tsk_get(thread->task_id);
        io_set_src(&thread->io_event_src, &t->io_event_src);
    }
    
    fatal_exception(thread->task_id, PG_IO_ERROR);
    
    rb_remove(&task->vmm_info.wait_root, &thread->vmm_info.pg_node);
    if(removeTBLTree)
        rb_remove_child(&task->vmm_info.tbl_wait_root, &thread->vmm_info.tbl_node);
    thread->flags &= ~THR_FLAG_PAGEFAULT;
}

/*
This function will remove all threads killed
and destroy it's task if it was also killed.
*/
void vmm_check_threads_pg(struct pm_thread **thr, BOOL removeTBLTree)
{
    struct pm_thread *thread = *thr, *athread, *curr_thr, *othr = NULL;
	struct pm_task *task, *t;
    int task_id = thread->task_id;
    ADDR pg_addr = thread->vmm_info.page_in_address;
    rbnode n = thread->vmm_info.pg_node;
    rbnode *currnode = NULL, *on = NULL;

    athread = thread;
    task = tsk_get(thread->vmm_info.fault_task);
    currnode = thread->vmm_info.pg_node.next;
    
	while(currnode)
	{
        curr_thr = thr_get(currnode->value2);
        t = tsk_get(curr_thr->task_id);

        on = currnode->next;
        
        curr_thr->flags &= ~(THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL);
		    
        if(curr_thr->state == THR_KILLED)
        {
            /* Thread was killed! */
			if(on && othr == NULL)
            {
                othr = thr_get(on->value2);
                othr->vmm_info.page_in_address = pg_addr;
            }

            if(athread == curr_thr)
            {
                athread = NULL;
            }
            else if(othr == NULL)
            {
                // set the IO source if it's not the same task as the thread
                if(curr_thr->task_id != task->id)
                    io_set_src(&curr_thr->io_event_src, &task->io_event_src);
                athread = curr_thr;
            }
            else
            {
                // set the IO source if it's not the same task as the thread
                if(curr_thr->task_id != task->id)
                    io_set_src(&othr->io_event_src, &task->io_event_src);
                athread = othr;
            }
               
            rb_remove_child(&task->vmm_info.wait_root, &curr_thr->vmm_info.pg_node);
            if(removeTBLTree)
                rb_remove_child(&task->vmm_info.tbl_wait_root, &curr_thr->vmm_info.tbl_node);
            
            /* this thread was killed it does not belong to the page fault task */
            if(thread->vmm_info.fault_task != thread->task_id)
            {
                // destroy the thread
                t = tsk_get(curr_thr->task_id);
                thr_destroy_thread(curr_thr->id);
                
                tsk_destroy(t);
            }   
        }
        else if(task->state == TSK_KILLED 
            && curr_thr->task_id != task_id)
        {
            // if the page fault task was killed, wake all other threads 
            // from other tasks.
            // - If the task is different and was accesed from an SMO, this should 
            //   fail on read/write mem syscalls.
            // - If the task is a shared library, in order for it to be killed
            //   this thread task must have been killed, then it'll fall on the 
            //   first condition.
		    curr_thr->state = THR_WAITING;
	        sch_activate(curr_thr);
        }

        currnode = on;
	}

    // restore the IO source it it's not the same task and our 
    // thread is dead
    if(athread != thread && thread->task_id != task->id)
    {
        t = tsk_get(thread->task_id);
        io_set_src(&thread->io_event_src, &t->io_event_src);
    }
    
    if(athread == NULL && pg_addr)
    {
        vmm_put_page(pg_addr);
        task->vmm_info.page_count--;
    }

    // if the original task was killed, then destroy it
    if(task->state == TSK_KILLED)
	{
        if ((task->flags & TSK_SHARED_LIB) == 0
            || ((task->flags & TSK_SHARED_LIB) && task->vmm_info.wait_root.root == NULL))
        {
              // it was a shared library waiting to be removed
            // or a task waiting to be killed
            tsk_destroy(task);
        }

        *thr = NULL;
        return;
	}
        
    *thr = athread;
}

/*
Re-enable Threads Waiting for the same page.
// vmm_check_threads_pg must be invoked before invoking this function.
*/
void vmm_wake_pf_threads(struct pm_thread *thread)
{
	struct pm_thread *curr_thr = NULL;
    struct pm_task *task = NULL;
    rbnode *currnode = NULL, *on = NULL;

	/* 
       First things first: Unblock the first 
       faulting thread and reactivate it. 
    */
	thread->state = THR_WAITING;	
	thread->flags &= ~(THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL);

    if(thread->task_id != thread->vmm_info.fault_task)
    {
        task = tsk_get(thread->task_id);
        io_set_src(&thread->io_event_src, &task->io_event_src);
        task = NULL;
    }

	sch_activate(thread);

	/* 
	Go through waiting threads and enable all threads waiting for the same page.
	*/
    currnode = thread->vmm_info.pg_node.next;
    
	while(currnode != NULL)
	{
        curr_thr = thr_get(currnode->value2);
        on = currnode->next;

        curr_thr->flags &= ~(THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL);
		curr_thr->state = THR_WAITING;
	    sch_activate(curr_thr);

		currnode = on;
	}

    task = tsk_get(thread->vmm_info.fault_task);
    rb_remove(&task->vmm_info.wait_root, &thread->vmm_info.pg_node); // remove al threads waiting for the page from the tree

	/* 
		If there are no threads on the vmm_info.swaptbl_next list unlock the table.
	*/
    rbnode *n = rb_search(&task->vmm_info.tbl_wait_root, TBL_ADDRESS(thread->vmm_info.fault_address));
	
	if(n)
	{
		/* remove IOLCK from the table */
		if(thread->vmm_info.tbl_node.next == NULL)
            vmm_set_flags(thread->task_id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(thread->task_id, thread->vmm_info.fault_address)), TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
        rb_remove(&task->vmm_info.tbl_wait_root, &thread->vmm_info.tbl_node); // remove al threads waiting for the page from the tree
        thread->vmm_info.tbl_node.next = NULL;
	}
}

