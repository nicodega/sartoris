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
#include "types.h"
#include <services/pmanager/services.h>
#include "task_thread.h"
#include "scheduler.h"
#include "io.h"
#include "interrupts.h"
#include "vm.h"
#include "layout.h"

static struct pm_thread *thread_info[MAX_THR];
static struct pm_thread pman_sched_thr;

void thr_init()
{
	UINT32 i;

	for (i=0; i<MAX_THR; i++) 
	{
		thread_info[i] = NULL;
	}
}

struct pm_thread *thr_create(UINT16 id, struct pm_task *task)
{
    if(thread_info[id] != NULL) return NULL;

    if(task != NULL && task->id == 0 && id == SCHED_THR)
        thread_info[id] = &pman_sched_thr;
    else
        thread_info[id] = (struct pm_thread *)kmalloc(sizeof(struct pm_thread));

    if(thread_info[id] == NULL) return NULL;

    struct pm_thread * thr = thread_info[id];

    thr->id = id;

    if(task == NULL) return thr;

    thr->state = THR_NOTHING;
    thr->flags = THR_FLAG_NONE;
    thr->next_thread = NULL;
    thr->vmm_info.fault_next_thread = NULL;
    thr->interrupt = 0;
    thr->vmm_info.page_displacement = 0;
    thr->vmm_info.page_in_address = NULL;

    thr->stack_addr = NULL;
    thr->vmm_info.swaptbl_next = NULL;
    thr->task_id = task->id;

    sch_init_node(&thr->sch);		

    io_init_source(&thr->io_event_src, FILE_IO_THREAD, id);
    io_init_event(&thr->io_finished, &thr->io_event_src);
    thr->swp_io_finished.callback = NULL;

    init_thr_signals(thr);

    vmm_init_thread_info(&thr->vmm_info);

    /* Fix thread list */
	if(task->first_thread != NULL)
		thr->next_thread = task->first_thread;
	
	task->first_thread = thr;
	task->num_threads++;

    return thr;
}

// gets the specified thread
struct pm_thread *thr_get(UINT16 id)
{
	if(id >= MAX_THR) return NULL;
	return thread_info[id];
}

UINT16 thr_get_id(UINT32 lower_bound, UINT32 upper_bound) 
{  
	UINT16 i;

	for (i = 1; i <= MAX_THR; i++) 
	{
		if (thread_info[i] == NULL) 
            return i;
	}

	return 0xFFFF;
}

/*
Returns
-1 if failed
0 if ok
>0 if threads where left on KILLING state because of page fault (i.e swap read pending)
*/
int thr_destroy_thread(UINT16 thread_id)
{
	struct pm_thread * thr = thr_get(thread_id);
	int ret = 0;

	/*
	NOTE: Don't think about resting VMM info on this function.
	It will be used if we already have a pending read/write of a page 
	for this thread, and thread must stay KILLED.
	*/

	if(thr == NULL) 
	{
		return -1;
	} 
	else 
	{
		if(thr->state != THR_KILLED && destroy_thread(thread_id) != SUCCESS) 
		{
			return -1;
		} 
		else 
		{
			struct pm_task *task = tsk_get(thr->task_id);

            if(task == NULL) return -1;

			if(thr->state != THR_KILLED) 
            {
                task->num_threads--;

			    /* Fix Task List */
			    if(task->first_thread == thr)
			    {
				    task->first_thread = thr->next_thread;
			    }
			    else
			    {
				    // add a prev thread !!! this is awful
				    struct pm_thread *currTrhead = task->first_thread;
				    while(currTrhead != NULL && currTrhead->next_thread != thr)
				    {
					    currTrhead = currTrhead->next_thread;
				    }
				    currTrhead->next_thread = thr->next_thread;
			    }
            }

			if(thr->state != THR_KILLED && (thr->flags & (THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL)))
			{
                /* Thread is waiting for a page fault (either swap or PF) */
				thr->state = THR_KILLED;
				ret++;
			}
			else
			{
                thread_info[thr->id] = NULL;
                kfree(thr);
                thr = NULL;
			}
			
			if(thr != NULL && thr->state == THR_KILLED) 
				task->killed_threads--;

			/* 
			If thread had pages in, they will be freed by swap manager 
			Should we implement files, or locked pages, a call to free them 
			must be issued here 
			*/
			if(thr != NULL)
			{
				if(thr->interrupt != 0)	
					int_dettach(thr);

				/* Remove thread from scheduler */
				sch_remove(thr);
			}

			return ret;
		}
	}
}

