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

#include "types.h"
#include <services/pmanager/signals.h>
#include "time.h"

struct pm_task;
struct pm_thread;

#ifndef PMANINT_SIGNALSH
#define PMANINT_SIGNALSH

extern UINT32 ticks;		// clock ticks, when it reaches 0xFFFFFFFF it will return to 0 again
extern INT32 direction;		// each time ticks overflows this will be set to !direction

/* A signal for a specified thread */
struct thr_signal
{
	struct pm_thread *thread;   // the thread this signal is for
	BYTE event_type;            // event which will trigger the signal
	BYTE id;                    // identifier
	UINT16 task;                // task from which the signal is expected
	UINT16 rec_type;			// type of recursion
	UINT32 signal_param;
	UINT16 signal_port;         // port where the signal response must be sent (including errors)

	TIME timeout;

    struct thr_signal *tnext;   // next signal on thread list
	struct thr_signal *tprev;
	struct thr_signal *gnext;   // next signal on global list
	struct thr_signal *gprev;
	struct thr_signal *inext;   // next on interrupt signals
} PACKED_ATT;

#define SIGNAL_REC_TYPE_NONE		0
#define SIGNAL_REC_TYPE_INFINITE	1
#define SIGNAL_REC_TYPE_REPEATING	2

/* This structure will hold signals for a given thread */
struct thr_signals
{
	struct thr_signal *blocking_signal; // there will only be one blocking signal at a given time
	struct thr_signal *first;           // first signal on the thread signals list 
	struct pm_thread *next;             // next thread with signals
	struct pm_thread *prev;             // prev thread with signals
    
    ADDR stack;                         // if this is not NULL it will contain the address of the stack the signals handler must use.	
    
    int pending_int;                    // If this is 1, the scheduler must run this thread handler_ep!
} PACKED_ATT;

/* This structure will hold signals information for a task */
struct tsk_signals
{
	ADDR handler_ep;                    // If not null, this will point to a user space address where an exception signals handler is located.
} PACKED_ATT;

/* Global signals container */
struct signals
{
	struct pm_thread *first_thr;        // first thread with waiting signals
	struct thr_signal *first;           // first signal on the global signals list
                                        // (signals will be ordered by timeout)
} PACKED_ATT;

extern struct signals signals;          // global signals container.

/* Init Global signals container */
void init_signals();
/* Init thread signals container */
void init_thr_signals(struct pm_thread *thr);
/* Handle incoming signal and event messages */
void process_signals_and_events();
/* Send signals whose timeout expired */
void send_signals();
/* Init task signals information */
void init_tsk_signals(struct pm_task *tsk);

void wait_signal(struct wait_for_signal_cmd *signal, BYTE blocking, UINT16 task);
void signal_nblock2block(struct wait_for_signal_cmd *signal_cmd, UINT16 task);
void discard_signal(struct discard_signal_cmd *dsignal, UINT16 task);
void event(struct event_cmd *evt, UINT16 task);

#define send_signal(signal,evt,ret) send_signal_ex(signal, evt, ret, NULL);
void send_signal_ex(struct thr_signal *signal, struct event_cmd *evt, INT32 ret, void *eaddr);
void insert_signal(struct thr_signal *signal, struct pm_thread *thread);
void remove_signal(struct thr_signal *signal, struct pm_thread *thread);
int signal_comp(struct thr_signal *s0, struct thr_signal *s1);
void remove_thr_signals(struct pm_thread *thread);
void set_signal_handler(struct set_signal_handler_cmd *cmd, UINT16 task);
void set_signal_stack(struct set_signal_stack_cmd *cmd, UINT16 task);

#endif

