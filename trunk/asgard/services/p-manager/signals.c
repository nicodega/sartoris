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

#include "pmanager_internals.h"

unsigned int ticks = 0;	// clock ticks, when it reaches 0xFFFFFFFF it will return to 0 again
int direction = 0;	// each time ticks overflows this will be set to !direction
struct signals signals;

extern struct pm_task   task_info[MAX_TSK];
extern struct pm_thread thread_info[MAX_THR];

/* Initialize signals container. */
void init_signals()
{
	signals.first_thr = -1;
	signals.first = NULL;

	int i = 0;
	for( ;i < MAX_THR; i++)
	{
		signals.tsignals[i].next = signals.tsignals[i].prev = -1;
		signals.tsignals[i].first = signals.tsignals[i].blocking_signal = NULL;
	}
	ticks = 0;
}


/* Handle incoming signal messages */
void process_signals()
{
	struct wait_for_signal_cmd signal;
	int id;

	/* Get signal requests from signals port */
	while(get_msg_count(PMAN_SIGNALS_PORT) > 0)
	{
		get_msg(PMAN_SIGNALS_PORT, &signal, &id);

		switch(signal.command)
		{
			case WAIT_FOR_SIGNAL:
				wait_signal(&signal, 1, id);
				break;
			case WAIT_FOR_SIGNAL_NBLOCK:
				wait_signal(&signal, 0, id);
				break;
			case DISCARD_SIGNAL:
				discard_signal(&signal, id);
				break;

		}
	}
}

/* Handle incoming events */
void process_events()
{
	struct event_cmd evt;
	int id;

	/* Get signal requests from signals port */
	while(get_msg_count(PMAN_EVENTS_PORT) > 0)
	{
		get_msg(PMAN_EVENTS_PORT, &evt, &id);

		if(evt.command == EVENT)
		{
			event(&evt, id);
		}
	}
}

/* Timer handling */
void timer_tick()
{
	unsigned int oticks = ticks;
	ticks++;

	if(oticks > ticks)
	{
		// overflow
		direction = !direction;
	}

	/* Now check for timeout on thread signals using global list */
	struct thr_signal *signal = signals.first;

	while(signal != NULL && signal->dir == direction && signal->timeout == ticks)
	{
		if(signal->task == PMAN_TASK && signal->event_type == PMAN_SLEEP)
		{
			// it was a sleep command
			struct event_cmd sleep_evt;
			sleep_evt.event_res0 = 0;
			sleep_evt.event_res1 = 0;
			send_signal(signal, &sleep_evt, SIGNAL_OK);
		}
		else
		{
			send_signal(signal, NULL, SIGNAL_TIMEOUT);
		}
		remove_signal(signal, signal->thread);
		signal = signals.first;
	}
}

/* Create a blocking signal for the process */
void wait_signal(struct wait_for_signal_cmd *signal, int blocking, int task)
{

	/* Now, the process might have sent multiple blocking signal requests 
	if it did, then the last signal expected will be the one unblocking 
	the thread.
	*/
	struct signal_cmd signal_msg;

	signal_msg.command = SIGNAL;
	signal_msg.thr_id = signal->thr_id;
	signal_msg.event_type = signal->event_type;
	signal_msg.id = signal->id;
	signal_msg.task = signal->task;
	signal_msg.res0 = 0;
	signal_msg.res1 = 0;
	signal_msg.ret = SIGNAL_FAILED;
	
	/* Create the signal struct */
	struct thr_signal *nsignal = (struct thr_signal *)malloc(sizeof(struct thr_signal));
	if(nsignal == NULL || signal->thr_id >= MAX_THR || thread_info[signal->thr_id].task_id != task
		  || thread_info[signal->thr_id].state == THR_NOTHING
		  || thread_info[signal->thr_id].state == THR_KILLED
		  || thread_info[signal->thr_id].state == THR_EXEPTION)
	{
		// fail
		send_msg(task, signal->signal_port, &signal_msg);
		return;
	}

	nsignal->thread = signal->thr_id;
	nsignal->event_type = signal->event_type;
	nsignal->id = signal->id;
	nsignal->task = signal->task;
	nsignal->signal_param0 = signal->signal_param0;
	nsignal->signal_param1 = signal->signal_param1;
	nsignal->signal_port = signal->signal_port;

	nsignal->infinite = 0;

	if(signal->timeout == PMAN_SIGNAL_TIMEOUT_INFINITE)
	{
		nsignal->infinite = 1;
	}
	else
	{
		unsigned int otimeout = ROUND_TIMEOUT(signal->timeout);

		nsignal->timeout = otimeout + ticks;

		if(nsignal->timeout < ticks || nsignal->timeout < otimeout)
		{
			nsignal->dir = !direction;
			nsignal->timeout = otimeout - (0xFFFFFFFF - ticks);
		}
		else
		{
			nsignal->dir = direction;
		}
	}

	nsignal->tnext = NULL;
	nsignal->gnext = NULL;
	nsignal->tprev = NULL;
	nsignal->gprev = NULL;

	/* Set blocking signal */
	if(blocking)
	{
		signals.tsignals[signal->thr_id].blocking_signal = nsignal;
	}

	/* Insert signal */
	insert_signal(nsignal, nsignal->thread);
}

void discard_signal(struct wait_for_signal_cmd *dsignal, int task)
{
	if(dsignal->thr_id >= MAX_THR || thread_info[dsignal->thr_id].task_id != task
		  || thread_info[dsignal->thr_id].state == THR_NOTHING
		  || thread_info[dsignal->thr_id].state == THR_KILLED
		  || thread_info[dsignal->thr_id].state == THR_EXEPTION)
	{
		// fail
		return;
	}

	/* Find a matching signal */
	struct thr_signal *signal = signals.tsignals[dsignal->thr_id].first;

	while(signal != NULL)
	{
		if(signal->task == dsignal->task && signal->event_type == dsignal->event_type
			&& (signal->signal_param0 == dsignal->signal_param0) 
			&& (signal->signal_param1 == dsignal->signal_param1) 
			&& signal->id == dsignal->id 
			&& signal->signal_port == dsignal->signal_port ) 
			break;
		signal = signal->tnext;
	}

	if(signal != NULL)
	{	
		remove_signal(signal, signal->thread);
		free(signal);
	}
}

void event(struct event_cmd *evt, int task)
{
	struct thr_signal *signal = NULL, *nsignal = NULL;

		/* Process events. 
		This will go through signals looking for a match. if event task is 
		PMAN_GLOBAL_EVENT all signals will be considered, if 
		PMAN_SIGNAL_PARAM_IGNORE is set, param won't be taken into account. */
	if(evt->task == PMAN_GLOBAL_EVENT)
	{
		// as inifinite timeout events are kept outside the 
		// global list, this will go through signals 
		// on each thread
		int currthr = signals.first_thr;

		while(currthr != -1)
		{
			/* Check all signals */
			signal = signals.tsignals[currthr].first;

			while(signal != NULL)
			{
				nsignal = signal->tnext;
				if(signal->task == task && signal->event_type == evt->event_type 
					&& (signal->signal_param0 == evt->param1 || signal->signal_param0 == (unsigned short)PMAN_SIGNAL_PARAM_IGNORE) 
					&& (signal->signal_param1 == evt->param2 || signal->signal_param1 == (unsigned short)PMAN_SIGNAL_PARAM_IGNORE))
				{
					// ok it matches!
					send_signal(signal, evt, SIGNAL_OK);
					remove_signal(signal, signal->thread);
					free(signal);
				}
				signal = nsignal;
			}

			/* Continue with next thread */
			currthr = signals.tsignals[currthr].next;
		}
	}
	else
	{
		if(task >= MAX_TSK) return;

		/* Go through threads of the specified task */
		int currthr = task_info[evt->task].first_thread;

		while(currthr != -1)
		{
			/* Check all signals */
			signal = signals.tsignals[currthr].first;

			while(signal != NULL)
			{
				nsignal = signal->tnext;
				if(signal->task == task && signal->event_type == evt->event_type 
					&& (signal->signal_param0 == evt->param1 || signal->signal_param0 == (unsigned short)PMAN_SIGNAL_PARAM_IGNORE) 
					&&  (signal->signal_param1 == evt->param2 || signal->signal_param1 == (unsigned short)PMAN_SIGNAL_PARAM_IGNORE))
				{
					// ok it matches!
					send_signal(signal, evt, SIGNAL_OK);
					remove_signal(signal, signal->thread);
					free(signal);
				}
				signal = nsignal;
			}

			/* Continue with next thread */
			currthr = thread_info[currthr].next_thread;
		}
	}
}

/* Send a signal message based on an event.
NOTE: This won't remove the signal, nor will it modify thread state, or perform any checks.
if evt is NULL it'll send a message with ret.
*/
void send_signal(struct thr_signal *signal, struct event_cmd *evt, int ret)
{
	struct signal_cmd signal_msg;

	signal_msg.command = SIGNAL;
	signal_msg.thr_id = signal->thread;
	signal_msg.event_type = signal->event_type;
	signal_msg.id = signal->id;
	signal_msg.task = signal->task;

	if(evt != NULL)
	{
		signal_msg.res0 = evt->event_res0;
		signal_msg.res1 = evt->event_res1;
		signal_msg.ret = ret;
	}
	else
	{
		signal_msg.res0 = 0;
		signal_msg.res1 = 0;
		signal_msg.ret = ret;
	}
	
	send_msg(thread_info[signal->thread].task_id, signal->signal_port, &signal_msg);
}

/* Insert a signal */
void insert_signal(struct thr_signal *signal, int thread)
{
	/* Insert on thread list */
	struct thr_signal *ofirst = signals.tsignals[thread].first;

	signals.tsignals[thread].first = signal;
	signal->tnext = ofirst;
	signal->tprev = NULL;

	if(ofirst != NULL) ofirst->tprev = signal;

	if(!signal->infinite)
	{
		/* Insert on global list (ordered) */
		struct thr_signal *prv = NULL, *curr = signals.first;

		while(curr != NULL && signal_comp(signal, curr) >= 0)
		{
			curr = curr->gnext;
		}

		/* prv should contain the previous signal, and curr the next one */
		ofirst = signals.first;

		if(prv != NULL)
		{
			prv->gnext = signal;
		}
		else
		{
			signals.first = signal;
			ofirst->gprev = signal;
		}

		if(curr != NULL)
		{
			curr->gprev = signal;
		}

		signal->gnext = curr;
		signal->gprev = prv;
	}

	/* If thread is not on the list, add it */
	if(signals.tsignals[thread].first->tnext == NULL)
	{
		int otfirst = signals.first_thr;

		signals.first_thr = thread;
		signals.tsignals[thread].next = otfirst;
		signals.tsignals[otfirst].prev = thread;
	}
}

/* Remove a signal (it won't free the signal structure */
void remove_signal(struct thr_signal *signal, int thread)
{
	/* If its the blocking signal, set to NULL */
	if(signals.tsignals[thread].blocking_signal == signal)
	{
		signals.tsignals[thread].blocking_signal = NULL;
	}

	/* Remove from global list */
	if(!signal->infinite)
	{
		if(signal->gprev != NULL)
		{
			signal->gprev->gnext = signal->gnext;
		}
		else
		{
			signals.first = signal->gnext;
		}
	}

	if(signal->gnext != NULL)
		signal->gnext->gprev = signal->gprev;
	
	/* Remove from thread list */
	if(signal->tprev != NULL)
	{
		signal->tprev->tnext = signal->tnext;
	}
	else
	{
		signals.tsignals[thread].first = signal->tnext;
	}
	if(signal->tnext != NULL)
		signal->tnext->tprev = signal->tprev;

	/* Check if the thread has any more signals 
	   and update first thread.
	*/
	if(signals.tsignals[thread].first == NULL)
	{
		if(signals.tsignals[thread].prev == -1)
		{
			signals.first_thr = signals.tsignals[thread].next;
		}
		else 
		{
			signals.tsignals[signals.tsignals[thread].prev].next = signals.tsignals[thread].next;
			if(signals.tsignals[thread].next != -1)
			{
				signals.tsignals[signals.tsignals[thread].next].prev = signals.tsignals[thread].prev;
			}
		}
		signals.tsignals[thread].next = -1;
		signals.tsignals[thread].prev = -1;
	}

	/* Update thread status */
	if(signals.tsignals[thread].blocking_signal == NULL)
	{
		thread_info[thread].state = THR_WAITING;	// no longer blocked
	}
}

/* Compare two signals, by using also global direction flag 
return: 0 equal, < 0 then s0 < s1, > 0 then s0 > s1.
*/
int signal_comp(struct thr_signal *s0, struct thr_signal *s1)
{
	if(s0->dir == s1->dir)
		return s0->timeout - s1->timeout;

	if(direction)
	{
		/* If direction is 1, then it means signals with dir 0 will be greater than signals with dir 1 */
		return s1->dir - s0->dir;
	}
	else
	{
		/* If direction is 0, then it means signals with dir 1 will be greater than signals with dir 0 */
		return s0->dir - s1->dir;
	}
}


