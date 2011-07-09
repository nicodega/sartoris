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


#include <lib/debug.h> // REMOVE

#include <services/pmanager/signals.h>
#include <lib/signals.h>
#include <lib/scheduler.h>
#include <os/pman_task.h>
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <lib/structures/list.h>
#include <lib/malloc.h>

#include <lib/critical_section.h>
static list signals;
static int initialized = 0;
static unsigned char last_id = 0;

#ifdef SIGNALS_MULTITHREADED
static struct mutex sigmutex;
#endif

static int signals_port = SIGNALS_PORT;

unsigned char get_id(int threadid);
SIGNALHANDLER prepare_send_signal(unsigned short event_type, unsigned short task, unsigned short param1, unsigned short param2, unsigned char id);
void process_signal_responses();


void init_signals()
{
	init(&signals);
	initialized = 1;

#ifdef SIGNALS_MULTITHREADED
	init_mutex(&sigmutex);
#endif
}

void set_signals_port(int port)
{
	signals_port = port;
}


void sleep(unsigned int miliseconds)
{
	wait_signal(PMAN_SLEEP, PMAN_TASK, miliseconds, SIGNAL_PARAM_IGNORE, SIGNAL_PARAM_IGNORE, NULL, NULL);
}

/* This function will wait for a signal. It will return 0 if signal timedout, 1 otherwise. 
param1 and param2 can be set to SIGNAL_IGNORE_PARAM and timeout to SIGNAL_TIMEOUT_INFINITE. */
int wait_signal(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned short param1, unsigned short param2, unsigned short *res0, unsigned short *res1)
{	
	struct wait_for_signal_cmd waitcmd;
	struct signal_cmd signal;
	int sender_id, res = 1;

	waitcmd.command = WAIT_FOR_SIGNAL;
	waitcmd.thr_id = get_current_thread();
	waitcmd.event_type = event_type;
	waitcmd.task = task;
	waitcmd.timeout = timeout;
	waitcmd.signal_param0 = param1;
	waitcmd.signal_param1 = param2;
	waitcmd.signal_port = signals_port;

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
	waitcmd.id = get_id(waitcmd.thr_id);
#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	SIGNALHANDLER sigh = prepare_send_signal(event_type, task, param1, param2, waitcmd.id);

	send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &waitcmd);

	for(;;)
	{
		int chkres = check_signal(sigh, res0, res1);

		if(chkres != 0)
		{
			struct signal_response *sentry = (struct signal_response *)get_at(sigh);
			remove_at(&signals, sigh);	// check signal won't discard it
			free(sentry);
			return chkres == -1;		// finished.
		}
        reschedule();
	}
}

SIGNALHANDLER wait_signal_async(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned short param1, unsigned short param2)
{
	struct wait_for_signal_cmd waitcmd;
	struct signal_cmd signal;
	int sender_id, res = 1;

	waitcmd.command = WAIT_FOR_SIGNAL_NBLOCK;
	waitcmd.thr_id = get_current_thread();
	waitcmd.event_type = event_type;
	waitcmd.task = task;
	waitcmd.timeout = timeout;
	waitcmd.signal_param0 = param1;
	waitcmd.signal_param1 = param2;
	waitcmd.signal_port = signals_port;
#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
	waitcmd.id = get_id(waitcmd.thr_id);
#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif
	SIGNALHANDLER sigh = prepare_send_signal(event_type, task, param1, param2, waitcmd.id);

	send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &waitcmd);

	return sigh;
}

/* This function will check if a signal has arrived. Returns 0 if no signal, -1 if timed out, 1 if signal arrived */
int check_signal(SIGNALHANDLER sigh, unsigned short *res0, unsigned short *res1)
{
	int res = 0;

    if(sigh == NULL) return 0;

	process_signal_responses();
	
#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
	struct signal_response *sentry = (struct signal_response *)get_at(sigh);

	if(sentry->received)
	{
		if(res0 != NULL) *res0 = sentry->res0;
		if(res1 != NULL) *res1 = sentry->res1;
		
		if(sentry->res == SIGNAL_TIMEOUT) 
			res = -1;
		else
			res = 1;
	}
#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	return res;
}

void discard_signal(SIGNALHANDLER sigh)
{
	struct signal_response *sentry = NULL;

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif

	sentry = (struct signal_response *)get_at(sigh);
	
	remove_at(&signals, sigh);

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	struct discard_signal_cmd discmd;

	discmd.command = DISCARD_SIGNAL;
	discmd.thr_id = sentry->thr;
	discmd.event_type = sentry->event_type;
	discmd.task = sentry->task;
	discmd.signal_param0 = sentry->param1;
	discmd.signal_param1 = sentry->param2;
	discmd.signal_port = signals_port;
	discmd.id = sentry->id;

	free(sentry);

	send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &discmd);

	process_signal_responses();	// just in case the signal was sent while we sent discard msg
}


void send_event(unsigned short task, unsigned short event_type, unsigned short param1, unsigned short param2, unsigned short res0, unsigned short res1)
{
	struct event_cmd evntcmd;
	struct signal_cmd signal;
	int sender_id, res = 1;

	evntcmd.command = EVENT;
	evntcmd.event_type = event_type;
	evntcmd.param1 = param1;
	evntcmd.param2 = param2;
	evntcmd.event_res0 = res0;
	evntcmd.event_res1 = res1;
	evntcmd.task = task;
	
	send_msg(PMAN_TASK, PMAN_EVENTS_PORT, &evntcmd);
}

/* Internal Helper functions*/

unsigned char get_id(int threadid)
{
	return last_id++;	// FIXME: find a free id on the signals list, for this thread
}

/* This function must be setup before sending the signal */
SIGNALHANDLER prepare_send_signal(unsigned short event_type, unsigned short task, unsigned short param1, unsigned short param2, unsigned char id)
{
	int currthr = get_current_thread();

	// create an entry on the signal waiting list
	CPOSITION entry = NULL, it = NULL;
	struct signal_response *sentry = NULL;

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif

	sentry = (struct signal_response *)malloc(sizeof(struct signal_response));

	if(sentry == NULL)
    {
#ifdef SIGNALS_MULTITHREADED
        leave_mutex(&sigmutex);
#endif
		return (SIGNALHANDLER)NULL;
    }

	sentry->thr = currthr;
	sentry->task = task;
	sentry->received = 0;
	sentry->param1 = param1;
	sentry->param2 = param2;
	sentry->event_type = event_type;
	sentry->id = id;

	// add signal on the list
	add_head(&signals, sentry);
	entry = get_head_position(&signals);

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	return (SIGNALHANDLER)entry;
}

/* Process signal responses */
void process_signal_responses()
{
	struct signal_cmd signal;
	int id;
	CPOSITION it = NULL;
	struct signal_response *sentry;

	while(get_msg_count(signals_port) > 0)
	{
		get_msg(signals_port, &signal, &id);

		if(id != PMAN_TASK) continue;	// ignore signals not received from pman
		
#ifdef SIGNALS_MULTITHREADED
		wait_mutex(&sigmutex);	// we enter the mutex here, because get_msg_count is safe for multithreading
#endif
		{
			// find the destination signal
			it = get_head_position(&signals);

			while(it != NULL)
			{
				sentry = (struct signal_response*)get_next(&it);

				if( sentry->thr == signal.thr_id 
					&& sentry->event_type == signal.event_type
					&& sentry->task == signal.task
					&& sentry->id == signal.id)
				{
					// signal received
					sentry->received = 1;
					sentry->res0 = signal.res0;
					sentry->res1 = signal.res1;
					sentry->res = signal.ret;
					break;
				}
			}
		}
#ifdef SIGNALS_MULTITHREADED
		leave_mutex(&sigmutex);
#endif
	}
}

