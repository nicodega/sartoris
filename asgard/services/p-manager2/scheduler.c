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


#include <sartoris/syscall.h>
#include "types.h"
#include "ports.h"
#include "scheduler.h"
#include "task_thread.h"
#include <drivers/pit/pit.h>

void sch_init()
{
	int i = 0;
	/* get a reasonable scheduling frequence */
	adjust_pit(SCHED_HERTZ);

	/* acknowledge first interrupt */
	ack_int_master();

	/* Initialize Scheduler List */
	for(i = 0; i < SCHED_MAXPRIORITY; i++)
	{
		scheduler.first[i] = NULL;
		scheduler.last[i] = NULL;		
	}

	scheduler.first_blocked = NULL;
	scheduler.running = NULL;
	scheduler.last_runned = NULL;
	scheduler.list_selector = 0;
	scheduler.total_threads = 0;
	scheduler.active_threads = 0;
}

/* Tis function will start next scheduled thread  */
int sch_schedule()
{	
	static BOOL intraised = FALSE;
	struct pm_thread *thread = NULL;
    struct pm_task *task = NULL;
	
	/* Deal with last_runned thread */
	if(scheduler.last_runned != NULL)
	{			
		/* If thread quantum is 0 place it at the end of it's priority list, and increase list selector */
		if(scheduler.last_runned->sch.quantums == 0)
		{
			/* Now thread is waiting (change status only if the thread has not changed state) */
			if(scheduler.last_runned->state == THR_RUNNING)
				scheduler.last_runned->state = THR_WAITING;
			else
				pman_print_and_stop("SCHED: Thread %i has state changed status: %i ", scheduler.last_runned->id, scheduler.last_runned->state);

			if(scheduler.first[scheduler.last_runned->sch.priority] != scheduler.last[scheduler.last_runned->sch.priority])
			{
				scheduler.first[scheduler.last_runned->sch.priority] = scheduler.last_runned->sch.next;
				scheduler.last_runned->sch.next->sch.prev = NULL;
				scheduler.last_runned->sch.next = NULL;

				scheduler.last[scheduler.last_runned->sch.priority]->sch.next = scheduler.last_runned;
				scheduler.last_runned->sch.prev = scheduler.last[scheduler.last_runned->sch.priority];
				scheduler.last[scheduler.last_runned->sch.priority] = scheduler.last_runned;
			}

			scheduler.last_runned->sch.quantums = sch_priority_quantum(scheduler.last_runned->sch.priority);
			scheduler.list_selector = (scheduler.list_selector == SCHED_MAXPRIORITY-1)? 0 : scheduler.list_selector + 1;
		}
	}

	/* Schedule next thread */
	
	// Find next list thread
	while(scheduler.first[scheduler.list_selector] == NULL) 
	{
		scheduler.list_selector = (scheduler.list_selector == SCHED_MAXPRIORITY-1)? 0 : scheduler.list_selector + 1;			
	}
	
	thread = scheduler.running = scheduler.first[scheduler.list_selector];
	thread->sch.quantums--; // spent a quantum
    	
	thread->state = THR_RUNNING;
	
	/* Run next thread */
    if(thread->signals.pending_int)
    {
        task = tsk_get(thread->task_id);

        // try to run the soft int
        // NOTE: If the thread is already on an int run_thread_int
        // will fail, then we must just run_thread.
        thread->signals.pending_int = run_thread_int(thread->id, task->signals.handler_ep, thread->signals.stack);
        if(thread->signals.pending_int)
        {
            run_thread(thread->id);
        }
    }
    else
    {
	    run_thread(thread->id);
    }

	/* Next time we are runned either by an int or run_thread, we will start here! */
	intraised = is_int0_active();

	if(intraised) 
		ack_int_master();    /* ack timer interrupt!*/

	scheduler.last_runned = scheduler.running;
	scheduler.running = NULL;
        
	return intraised;
}

/*
	Send a Thread onto the blocked list
*/
void sch_deactivate(struct pm_thread *thr)
{	
	if(thr->sch.recursion > 0)
	{
        thr->sch.recursion++;
		return;
	}

	thr->sch.recursion++;

	if(thr->sch.blocked == TRUE) return;	
	
	/* Remove From Scheduling list */
	sch_remove(thr);
	
	thr->sch.blocked = TRUE;

    // add to cheduler blocked list
	if(scheduler.first_blocked == NULL)
	{
		scheduler.first_blocked = thr;
		thr->sch.next = NULL;
		thr->sch.prev = NULL;	
	}
	else
	{
		scheduler.first_blocked->sch.prev = thr;
		thr->sch.next = scheduler.first_blocked;
		thr->sch.prev = NULL;
		scheduler.first_blocked = thr;
	}
	scheduler.active_threads--;
}

/* Remove a Thread from the blocked list */
void sch_activate(struct pm_thread *thr)
{	
	if(thr->sch.recursion > 1)
	{
		thr->sch.recursion--;
		return;
	}

	if(thr->sch.recursion != 0) thr->sch.recursion--;

	if(thr->sch.blocked != TRUE) return;

	thr->sch.blocked = FALSE;

	/* Remove the Thread from blocked list */
	if(thr == scheduler.first_blocked)
	{
		scheduler.first_blocked = scheduler.first_blocked->sch.next;
		if(scheduler.first_blocked != NULL)
			scheduler.first_blocked->sch.prev = NULL;
	}
	else
	{
		if(thr->sch.next != NULL) thr->sch.next->sch.prev = thr->sch.prev;
		if(thr->sch.prev != NULL) thr->sch.prev->sch.next = thr->sch.next;
	}

	/* Place the thread at the end of its priority list */
	if(scheduler.first[thr->sch.priority] != NULL)
	{
		scheduler.last[thr->sch.priority]->sch.next = thr;
		thr->sch.next = NULL;
		thr->sch.prev = scheduler.last[thr->sch.priority];		
	}
	else
	{
		scheduler.first[thr->sch.priority] = thr;
		thr->sch.next = NULL;
		thr->sch.prev = NULL;
	}
	scheduler.last[thr->sch.priority] = thr;
	thr->sch.quantums = sch_priority_quantum(thr->sch.priority);
	scheduler.active_threads++;
}

// add a thread to the scheduler as inactive
void sch_add(struct pm_thread *thr)
{
	thr->sch.blocked = TRUE;
	if(scheduler.first_blocked == NULL)
	{
		scheduler.first_blocked = thr;
		thr->sch.next = NULL;
		thr->sch.prev = NULL;
	}
	else
	{
		scheduler.first_blocked->sch.prev = thr;
		thr->sch.next = scheduler.first_blocked;
		thr->sch.prev = NULL;
		scheduler.first_blocked = thr;
	}
	scheduler.total_threads++;
}

// remove a task from scheduler
void sch_remove(struct pm_thread *thr)
{		
	if(!thr->sch.blocked)
	{
		/* Remove from its priority list */
		if(scheduler.last[thr->sch.priority] == thr)
			scheduler.last[thr->sch.priority] = thr->sch.prev;
		
		if(scheduler.first[thr->sch.priority] == thr)
			scheduler.first[thr->sch.priority] = thr->sch.next;
		
		if(thr->sch.next != NULL) thr->sch.next->sch.prev = thr->sch.prev;
		if(thr->sch.prev != NULL) thr->sch.prev->sch.next = thr->sch.next;
		
		scheduler.active_threads--;
	}
	else
	{
		/* remove from blocked list */
		if(scheduler.first_blocked == thr)
			scheduler.first_blocked = thr->sch.next;
		if(thr->sch.next != NULL)
			thr->sch.next->sch.prev = thr->sch.prev;
	}

	thr->sch.next = thr->sch.prev = NULL;

	if(scheduler.last_runned == thr)
	{		
		scheduler.last_runned = NULL;
		scheduler.list_selector = scheduler.list_selector+1 % SCHED_MAXPRIORITY;
	}
	else if(scheduler.running == thr) 
	{
		scheduler.running = NULL;
		scheduler.list_selector = scheduler.list_selector+1 % SCHED_MAXPRIORITY;
	}

	scheduler.total_threads--;
}

void sch_init_node(struct sch_node *n)
{
	n->next = NULL;
	n->prev = NULL;
	n->priority = SCHED_DEFAULTPRIORITY;
	n->quantums = sch_priority_quantum(n->priority);
	n->recursion = 0;
}

UINT16 sch_priority_quantum(int priority)
{
	switch(priority)
	{
		case 0:
			return 1;
			break;
		case 1:
			return 2;
			break;
		case 2:
			return 4;
			break;
		case 3:
			return 8;
			break;
		case 4:
			return 16;
			break;
		case 5:
		default: 
			return 32;
			break;
	}
}

/*
Returns the running thread.
*/
UINT16 sch_running()
{
	if(scheduler.running == NULL) return 0xFFFF;

	return scheduler.running->id;
}

/*
This function will tell the process manager, the given thread must run on
possition schedules at most
*/
void sch_reschedule(struct pm_thread *thr, UINT32 possition)
{
	struct pm_thread *thread = scheduler.first[thr->sch.priority];
	UINT32 i = 0, lsdiff;

	/* If thread is blocked (this should not happen.. but it can happen) don't reschedule */
	if(thread->state != THR_WAITING) return;
	
	/* 
	Place the thread at the given possition. 
	*/
	while(thread != NULL && i < possition)
	{
		thread = thread->sch.next;
		i++;
	}

	if(thread == thr)
	{
		/* Already there */
		return;
	}

	/* Remove thread from list */
	sch_remove(thr);

	if(thread == NULL)
	{
		/* Reached end of list, place at the end */
		if(thr == scheduler.last[thr->sch.priority]) return;
        
		scheduler.last[thr->sch.priority]->sch.next = thr;
		thr->sch.prev = scheduler.last[thr->sch.priority];
		thr->sch.next = NULL;
	}
	else if(thread == scheduler.first[thr->sch.priority])
	{
		/* Place at the begining */
		if(thr == scheduler.first[thr->sch.priority]) return;
		scheduler.first[thr->sch.priority]->sch.prev = thr;
		thr->sch.prev = NULL;
		thr->sch.next = scheduler.first[thr->sch.priority];
	}
	else
	{
		/* Place thread at new possition */
		thr->sch.prev = thread->sch.prev;
		thr->sch.next = thread;
		thread->sch.prev->sch.next = thr;
		thread->sch.prev = thr;
	}

	/* rearrange list_selector */
	scheduler.list_selector = thr->sch.priority;

	/* Return it's quantums to the thread */
	thr->sch.quantums = sch_priority_quantum(thr->sch.priority);
}

/* Force executing thread completion and send it to the end of the list. */
void sch_force_complete()
{
	if(scheduler.last_runned == NULL) return;

	/* Set quantum as finished, and place at the end of the list */
	if(scheduler.first[scheduler.last_runned->sch.priority] != scheduler.last[scheduler.last_runned->sch.priority])
	{
		scheduler.first[scheduler.last_runned->sch.priority] = scheduler.last_runned->sch.next;
		scheduler.last_runned->sch.next->sch.prev = NULL;
		scheduler.last_runned->sch.next = NULL;

		scheduler.last[scheduler.last_runned->sch.priority]->sch.next = scheduler.last_runned;
		scheduler.last_runned->sch.prev = scheduler.last[scheduler.last_runned->sch.priority];
		scheduler.last[scheduler.last_runned->sch.priority] = scheduler.last_runned;
	}

	/* Reset quantums */
	scheduler.last_runned->sch.quantums = sch_priority_quantum(scheduler.last_runned->sch.priority);
	scheduler.last_runned = NULL;
	scheduler.list_selector = (scheduler.list_selector == SCHED_MAXPRIORITY-1)? 0 : scheduler.list_selector + 1;
}
/* 
This function will process messages from sartoris events.
*/
void sch_process_portblocks()
{
    int id;
    struct evt_msg msg;

    while(get_msg_count(SARTORIS_EVENTS_PORT) > 0)
    {
        get_msg(SARTORIS_EVENTS_PORT, &msg, &id);

        if(msg.evt == SARTORIS_EVT_MSG || msg.evt == SARTORIS_EVT_PORT_CLOSED)
        {
            struct pm_task *tsk = tsk_get(msg.id);
            struct pm_thread *thr = tsk->first_thread;
            int i;
            unsigned int mask = 0;
            
            // wake threads waiting for a message on the port
            while(thr)
            {
                if(thr->block_port_mask & (0x1 << msg.param))
                {
                    thr->flags &= ~THR_FLAG_BLOCKED_PORT;

                    if(!thr->flags)
                        thr->state = THR_RUNNING;

                    thr->block_port_mask = 0;
                    sch_activate(thr);
                }
                                
                // fix the task port_blocks array
                for(i = 0; i < 32; i++)
                {
                    if(thr->block_port_mask & (0x1 << i))
                        tsk->port_blocks[i]--;
                }
            
                thr = thr->next_thread;
            }

            for(i = 0; i < 32; i++)
            {
                if(tsk->port_blocks[i])
                    mask |= (0x1 << i);
            }

            // set the wait again
            if(mask)
            {
                evt_wait(tsk->id, SARTORIS_EVT_MSG, mask);
            }
        }
    }
}

