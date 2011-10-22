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

extern void _exit(int);

static int initialized = 0;
static unsigned char last_id = 0;
sighandler_t handlers[256+32];
struct signal_response *signals[256+32];
unsigned char ids[32];
int has_handler = 0;

// this structure will be used to place pending exceptions 
// for a different thread on a list.
struct _pending_exe
{
    int thread;
    int exception;
    void *addr;
    struct _pending_exe *next;
    struct _pending_exe *prev;
} pending_exe;

pending_exe *pe_first = NULL;
signals_thr_info *thr_first = NULL; // informatin for each thread

#ifdef SIGNALS_MULTITHREADED
static struct mutex sigmutex;
#endif

static int signals_port = SIGNALS_PORT;

unsigned char get_id(int threadid);
void free_id(unsigned char id);
SIGNALHANDLER prepare_send_signal(unsigned short event_type, unsigned short task, unsigned int param, unsigned char id);
void process_signal_responses(int handler);

void init_signals()
{
    int i = 0;
	
    for(; i < 288; i++)
    {
        handlers[i] = SIG_DFL;
        signals[i] = NULL;
    }

    for(i = 0; i < 32; i++)
        ids[i] = 0;

#ifdef SIGNALS_MULTITHREADED
	init_mutex(&sigmutex);
#endif

    initialized = 1;
}

void set_signals_port(int port)
{
	signals_port = port;
}

void sleep(unsigned int miliseconds)
{
	wait_signal(PMAN_SLEEP, PMAN_TASK, miliseconds, SIGNAL_PARAM_IGNORE, NULL);
}

/* This function will wait for a signal. It will return 0 if signal timedout, 1 otherwise. 
param1 and param2 can be set to SIGNAL_IGNORE_PARAM and timeout to SIGNAL_TIMEOUT_INFINITE. */
int wait_signal(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned int param, unsigned int *res)
{	
	struct wait_for_signal_cmd waitcmd;
	struct signal_cmd signal;
	int sender_id, index;

	waitcmd.command = WAIT_FOR_SIGNAL;
	waitcmd.thr_id = get_current_thread();
	waitcmd.event_type = event_type;
	waitcmd.task = task;
	waitcmd.timeout = timeout;
	waitcmd.signal_param = param;
	waitcmd.signal_port = signals_port;

	waitcmd.id = get_id(waitcmd.thr_id);

	SIGNALHANDLER sigh = prepare_send_signal(event_type, task, param, waitcmd.id);

	send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &waitcmd);

	for(;;)
	{
		int chkres = check_signal(sigh, res);

		if(chkres != 0)
		{
            index = (((unsigned int)sigh - (unsigned int)signals) >> 2) + 32;

#ifdef SIGNALS_MULTITHREADED
	        wait_mutex(&sigmutex);
#endif      
            sentry = signals[index];
	        signals[index] = NULL;

#ifdef SIGNALS_MULTITHREADED
	        leave_mutex(&sigmutex);
#endif
			free(sentry);
			return chkres == -1;		// finished.
		}
        reschedule();
	}
}

SIGNALHANDLER wait_signal_async(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned int param)
{
	struct wait_for_signal_cmd waitcmd;

	waitcmd.command = WAIT_FOR_SIGNAL_NBLOCK;
	waitcmd.thr_id = get_current_thread();
	waitcmd.event_type = event_type;
	waitcmd.task = task;
	waitcmd.timeout = timeout;
	waitcmd.signal_param = param;
	waitcmd.signal_port = signals_port;

	waitcmd.id = get_id(waitcmd.thr_id);

	SIGNALHANDLER sigh = prepare_send_signal(event_type, task, param, waitcmd.id);

	send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &waitcmd);

	return sigh;
}

/* This function will check if a signal has arrived. Returns 0 if no signal, -1 if timed out, 1 if signal arrived */
int check_signal(SIGNALHANDLER sigh, unsigned int *res)
{
	int ret = 0;

    if(sigh == NULL
        || (unsigned int)sigh < (unsigned int)signals 
        || (unsigned int)sigh > (unsigned int)signals + 1024
        || (((unsigned int)sigh - (unsigned int)signals) & 3)) 
        return 0;

	process_signal_responses(0);
	
#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
	struct signal_response *sentry = signals[((unsigned int)sigh - (unsigned int)signals) >> 2];

	if(sentry->received)
	{
		if(res != NULL) *res = sentry->res;
		
		if(sentry->ret == SIGNAL_TIMEOUT) 
			ret = -1;
		else
			ret = 1;
	}
#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	return ret;
}

void discard_signal(SIGNALHANDLER sigh)
{
	struct signal_response *sentry = NULL;
    int index;

    if(sigh == NULL
        || (unsigned int)sigh < (unsigned int)signals 
        || (unsigned int)sigh > (unsigned int)signals + 1024
        || (((unsigned int)sigh - (unsigned int)signals) & 3)) 
        return 0;

    index = (((unsigned int)sigh - (unsigned int)signals) >> 2);

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
    
	sentry = signals[index];
	
	signals[index] = NULL;
    handlers[index] = NULL;

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

    if(!sentry) return;

	struct discard_signal_cmd discmd;

	discmd.command = DISCARD_SIGNAL;
	discmd.thr_id = sentry->thr;
	discmd.event_type = sentry->event_type;
	discmd.task = sentry->task;
	discmd.signal_param = sentry->param;
	discmd.signal_port = signals_port;
	discmd.id = sentry->id;

    if(sentry->received == 0)
    {
	    send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &discmd);

	    process_signal_responses(0);	// just in case the signal was sent while we sent discard msg
    }
    
    free_id(sentry->id);
	free(sentry);
}

void send_event(unsigned short task, unsigned short event_type, unsigned int param, unsigned int res)
{
	struct event_cmd evntcmd;

	evntcmd.command = EVENT;
	evntcmd.event_type = event_type;
	evntcmd.param = param;
	evntcmd.event_res = res;
	evntcmd.task = task;
	
	send_msg(PMAN_TASK, PMAN_EVENTS_PORT, &evntcmd);
}

/* Internal Helper functions*/
unsigned char get_id(int threadid)
{
#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif

    int i = 1, j = 0;  // first 32 signals are reserved

    while(ids[i] == 0xFFFFFFFF && i < 32){i++;}

    if(i == 32)
    {
#ifdef SIGNALS_MULTITHREADED
    	leave_mutex(&sigmutex);
#endif
        return -1;
    }

    while(ids[i] & (0x1 << j))
    {
        j++;
    }

    ids[i] |= (0x1 << j);

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

    // found a free id
	return (i << 5) + (32 - j);
}

void free_id(unsigned char id)
{
    int i = (id >> 5);
    int j = (31 - (id & 31));

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif    

    ids[i] &= ~(0x1 << j);

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif
}

/* This function must be setup before sending the signal */
SIGNALHANDLER prepare_send_signal(unsigned short event_type, unsigned short task, unsigned int param, unsigned char id)
{
	int currthr = get_current_thread();

	// create an entry on the signal waiting list
	CPOSITION entry = NULL, it = NULL;
	struct signal_response *sentry = NULL;

	sentry = (struct signal_response *)malloc(sizeof(struct signal_response));

	if(sentry == NULL)
    	return (SIGNALHANDLER)NULL;
    
	sentry->thr = currthr;
	sentry->task = task;
	sentry->received = 0;
	sentry->param = param;
	sentry->event_type = event_type;
	sentry->id = id;
    sentry->exec = 0;

#ifdef SIGNALS_MULTITHREADED
	wait_mutex(&sigmutex);
#endif
	// add signal on the list
	signals[id] = sentry;

#ifdef SIGNALS_MULTITHREADED
	leave_mutex(&sigmutex);
#endif

	return &signals[id];
}

/* Process signal responses */
void process_signal_responses(int handler)
{
    int currthr = get_current_thread();
	struct signal_cmd signal;
	int id;
	CPOSITION it = NULL;
	struct signal_response *sentry;
    pending_exe *ex = NULL;

    if(handler && pe_first)
    {
        ex = pe_first;
        while(ex)
        {
            if(handlers[ex->exception] && ex->thr_id == currthr)
            {
                handlers[ex->exception](ex->exception);

                /* Remove the exception from the list */
                if(ex->prev)
                    ex->prev->next = ex->next;
                else
                    pe_first = ex->next;
                if(ex->next)
                    ex->next->prev = ex->prev;
            }
            ex = ex->next;
        }
    }
    
    int msgs = get_msg_count(signals_port);

	while(msgs > 0)
	{
		get_msg(signals_port, &signal, &id);

		if(id != PMAN_TASK || signal->command != SIGNAL)
        {
            if(signal->command == SET_SIGNAL_STACK)
            {
                struct set_signal_stack_res *ss_res = (struct set_signal_stack_res*)signal;
                signals_thr_info *ti = NULL;                
#ifdef SIGNALS_MULTITHREADED
                wait_mutex(&sigmutex);
#endif
                while(ti)
                {
                    if(ti->thr_id == ss_res->thr_id)
                    {
                        ti->state = ((ss_res->result == SIGNAL_OK)? SIGNALS_THRINFO_STATE_ACK : SIGNALS_THRINFO_STATE_ERR);
                        break;
                    }
                    ti = ti->next;
                }
#ifdef SIGNALS_MULTITHREADED
                leave_mutex(&sigmutex);
#endif
            }
            msgs--;
            continue;	// ignore signals not received from pman 
                        // and responses to SET_SIGNAL_HANDLER
        }
		
#ifdef SIGNALS_MULTITHREADED
		wait_mutex(&sigmutex);	// we enter the mutex here, because get_msg_count is safe for multithreading
#endif
		{
            if(signal.event_type = PMAN_EXCEPTION && signal.task = PMAN_GLOBAL_EVENT)
            {
                // these signals won't have an sentry..
                if(handlers[signal.id] && handlers[signal.id] != SIG_IGN)
                {
                    if(handler && signal.thr_id == currthr)
                    {
                        handlers[signal.id](signal.id);
                    }
                    else
                    {
                        // add to the pending exceptions list
                        ex = (pending_exe*)malloc(sizeof(pending_exe));

                        if(!ex)
                            _exit(-1);

                        ex->exception = signal->res;
                        ex->addr = signal->eaddr;
                        ex->thread = signal->thr_id;

                        ex->next = pe_first;
                        ex->prev = NULL;
                        if(pe_first)
                            pe_first->prev = ex;
                        pe_first = ex;
                    }
                    msgs--;
                    continue;
                }
                else
                {
                    // an uncaught exception
#ifdef SIGNALS_MULTITHREADED
                    leave_mutex(&sigmutex);
#endif
                    _exit(-1);
                    return;
                }
            }
            else
            {
                signal.id -= 32;
            }

            // see if the signal was sent
            if(signals[signal.id])
            {
                sentry = signals[signal.id];

                if( sentry->thr == signal.thr_id 
					&& sentry->event_type == signal.event_type
					&& sentry->task == signal.task
					&& sentry->id == signal.id)
				{
					// signal received
					sentry->received = 1;
					sentry->res = signal.res;
					sentry->ret = signal.ret;
				}
#ifdef SIGNALS_MULTITHREADED
                leave_mutex(&sigmutex);
#endif
                if(handlers[id] && handler && signal.thr_id == currthr)
                {
                    handlers[id](id);
                }
            }
#ifdef SIGNALS_MULTITHREADED
            else
            {
                leave_mutex(&sigmutex);
            }
#endif
		}
        msgs--;
	}
}

/*
This will be executed as a soft int by pman.
*/
void sig_handler()
{
    process_signal_responses(1);
    ret_from_int();
}

int signal_id(SIGNALHANDLER sigh)
{
	struct signal_response *sentry = NULL;
    int index;

    if(sigh == NULL
        || (unsigned int)sigh < (unsigned int)signals 
        || (unsigned int)sigh > (unsigned int)signals + 1024
        || (((unsigned int)sigh - (unsigned int)signals) & 3)) 
        return 0;

    index = (((unsigned int)sigh - (unsigned int)signals) >> 2);

    return index + 32;
}

/*
This functions allow setting up a signal and raising it.
*/
sighandler_t signal(int id, sighandler_t handler)
{
    if(id < 0 || id > 287 || handler == NULL || handler == SIG_ERR) return SIG_ERR;

    // if the signal has not yet been set, set it
    if(id > 32)
    {
        if(!signals[id])
        {
            struct wait_for_signal_cmd waitcmd;

	        waitcmd.command = WAIT_FOR_SIGNAL_NBLOCK;
	        waitcmd.thr_id = get_current_thread();
	        waitcmd.event_type = 0;
	        waitcmd.task = get_current_task();
	        waitcmd.timeout = PMAN_SIGNAL_REPEATING;
	        waitcmd.signal_param = 0;
	        waitcmd.signal_port = signals_port;
	        waitcmd.id = id-32;

	        SIGNALHANDLER sigh = prepare_send_signal(event_type, task, param, waitcmd.id);

	        send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &waitcmd);
        }
        else if(signals[id] && (handler == SIG_IGN || handler == SIG_DFL))
        {
            discard_signal(&signals[id]);
        }
    }

    if(handler == SIG_DFL || handler == SIG_IGN)
        handlers[id] = NULL;
    else
        handlers[id] = handler;

    struct set_signal_handler_cmd cmd;

    if(!has_handler)
    {
        cmd.command = SET_SIGNAL_HANDLER;
        cmd.ret_port = signals_port;
        cmd.exceptions_port = signals_port;
        cmd.handler_ep = sig_handler;
        
        send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &cmd);
        has_handler = 1;
    }

    return handler;
}

void *get_stack()
{
    int currthr = get_current_thread();
    signals_thr_info *ti = NULL;

#ifdef SIGNALS_MULTITHREADED
    wait_mutex(&sigmutex);
#endif
    ti = thr_first;
    
    while(ti)
    {
        if(ti->thr_id == currthr) break;
        ti = ti->next;
    }
#ifdef SIGNALS_MULTITHREADED
    leave_mutex(&sigmutex);
#endif
    if(ti)
        return ti->stack_info.ss_sp;
    return NULL;
}

int raise(int id)
{
    if(id >= 0 && id < 288 && handlers[id])
    {
        if(id > 32)
        {
            send_event(get_current_task(), signals[id]->event_type, signals[id]->param, 0);
            reschedule();   // this will guarrantee if the signal is on the same thread, 
                            // the handler will be invoked on a blocking fashion.
        }
        else
        {
            // if this thread has a stack specified 
            // we need to swap to that stack
            void *ss = get_stack();

            if(ss)
                call_with_ss_swap((void*)handlers[id], ss);
            else
                handlers[id](id);            
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

/*
Set signals stack for current thread
*/
int sigaltstack(const stack_t *restrict ss, stack_t *restrict oss)
{
    // first see if this thread already has an entry
    int currthr = get_current_thread();
    signals_thr_info *ti = NULL;
    int isnew = 0;
    
    if(ss && (ss->ss_size & 0x00000FFF) != 0)
        ss->ss_size += (0x1000 - (ss->ss_size & 0x00000FFF));
    
#ifdef SIGNALS_MULTITHREADED
    wait_mutex(&sigmutex);
#endif
    ti = thr_first;
    
    while(ti)
    {
        if(ti->thr_id == currthr) break;
        ti = ti->next;
    }

    if(ti)
    {
        if(oss)
            *oss = *((stack_t*)ti);
        
        if(ti->state != SIGNALS_THRINFO_STATE_OK)
        {
#ifdef SIGNALS_MULTITHREADED
            leave_mutex(&sigmutex);
#endif
            return -1;
        }

        // remove it from the list and change it's status
        if(ti->prev)
            ti->prev->next = ti->next;
        else
            thr_first = ti->next;
        if(ti->next)
            ti->next->prev = ti->next;
        ti->state = SIGNALS_THRINFO_STATE_PENDING;
    }
    else
    {
        if(oss)
        {
            oss->ss_sp = NULL;
            oss->ss_size = 0;
            oss->ss_flags = 0;
        }

        isnew = 1;

        // create a new struct
        ti = (signals_thr_info*)malloc(sizeof(signals_thr_info));

        if(ti == NULL) 
        {
#ifdef SIGNALS_MULTITHREADED
            leave_mutex(&sigmutex);
#endif
            return -1;
        }
        ti->thr_id = currthr;
    }

    // add it as the head of the list
    ti->state = SIGNALS_THRINFO_STATE_PENDING;
        
    ti->next = thr_first;
    ti->prev = NULL;
    if(thr_first)
        thr_first->prev = ti;
    thr_first = ti;

#ifdef SIGNALS_MULTITHREADED
    leave_mutex(&sigmutex);
#endif

    /*
    Tell pman to change our stack.
    */
    set_signal_stack_cmd cmd;

    cmd.command = SET_SIGNAL_STACK;
    cmd.thr_id = currthr;
    cmd.stack = (SS == NULL)? NULL : ss->ss_sp;
    cmd.size = (SS == NULL)? 0 : (ss->ss_size >> 12);
    cmd.ret_port  = signals_port;

    send_msg(PMAN_TASK, PMAN_SIGNALS_PORT, &cmd);

    while(ti->state == SIGNALS_THRINFO_STATE_PENDING)
    {
        process_signal_responses(0);
        reschedule();
        if(ti->state == SIGNALS_THRINFO_STATE_ERR)
        {
            if(!isnew)
            {
                ti->state = SIGNALS_THRINFO_STATE_OK;
            }
            else
            {
                // remove the new struct from the list
#ifdef SIGNALS_MULTITHREADED
                wait_mutex(&sigmutex);
#endif
                if(ti->prev)
                    ti->prev->next = ti->next;
                else
                    thr_first = ti->next;
                if(ti->next)
                    ti->next->prev = ti->next;
#ifdef SIGNALS_MULTITHREADED
                leave_mutex(&sigmutex);
#endif
                free(ti);
            }
            return -1;
        }
    }

    // complete ti information
    ti->stack_info.ss_sp = cmd.stack;
    ti->stack_info.ss_size = cmd.size;
    ti->stack_info.ss_flags = 0;
    ti->state = SIGNALS_THRINFO_STATE_OK;

    return 0;
}
