
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "types.h"
#include <services/pmanager/services.h>
#include "task_thread.h"
#include "scheduler.h"
#include "io.h"
#include "interrupts.h"

static struct pm_thread thread_info[MAX_THR];

void thr_init()
{
	UINT32 i;

	for (i=0; i<MAX_THR; i++) 
	{
		thread_info[i].id = i;
		thread_info[i].state = THR_NOTHING;
		thread_info[i].flags = THR_FLAG_NONE;
		thread_info[i].next_thread = NULL;
		thread_info[i].vmm_info.fault_next_thread = NULL;
		thread_info[i].interrupt = -1;
		thread_info[i].vmm_info.page_displacement = 0;
		thread_info[i].vmm_info.page_in_address = NULL;

		thread_info[i].stack_addr = NULL;
		thread_info[i].vmm_info.swaptbl_next = NULL;
		thread_info[i].task_id = -1;

		sch_init_node(&thread_info[i].sch);		

		io_init_source(&thread_info[i].io_event_src, FILE_IO_THREAD, i);
		io_init_event(&thread_info[i].io_finished, &thread_info[i].io_event_src);
		thread_info[i].swp_io_finished.callback = NULL;

		init_thr_signals(&thread_info[i]);
	}
}


// gets the specified thread
struct pm_thread *thr_get(UINT16 id)
{
	if(id >= MAX_THR) return NULL;
	return &thread_info[id];
}

UINT16 thr_get_id(UINT32 lower_bound, UINT32 upper_bound) 
{  
	UINT16 i;

	for (i = 1; i <= MAX_THR; i++) 
	{
		if (thread_info[i].state == THR_NOTHING) 
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

	if(thr == NULL || thr->state == THR_NOTHING) 
	{
		return -1;
	} 
	else 
	{
		if((thr->state != THR_KILLED) && destroy_thread(thread_id) != SUCCESS) 
		{
			return -1;
		} 
		else 
		{
			struct pm_task *task = tsk_get(thr->task_id);

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

			if(!(thr->state != THR_KILLED) && (thr->flags & (THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL)))
			{
                /* Thread is waiting for a page fault (either swap or PF) */
				thr->state       = THR_KILLED;
				ret++;
			}
			else
			{
				thr->state       = THR_NOTHING;
				thr->task_id     = 0xFFFF;
				thr->next_thread = NULL;
			}
			
			if(thr->state == THR_KILLED && task->killed_threads > 0) 
				task->killed_threads--;

			/* 
			If thread had pages in, they will be freed by swap manager 
			Should we implement files, or locked pages, a call to free them 
			must be issued here 
			*/
			if(!(thr->state != THR_KILLED))
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

