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
	UINT16 infinite;
	UINT32 signal_param;
	UINT16 signal_port;         // port where the signal response must be sent (including errors)

	UINT32 timeout;             // timeout in sched ticks (using ROUND_TIMEOUT(x) to convert from milliseconds) 
                                // plus ticks (it could be less because of overflow but that is taken onto account). 
	INT32 dir;                  // if timeout + ticks overflows, this will be set to !direction

    struct thr_signal *tnext;   // next signal on thread list
	struct thr_signal *tprev;
	struct thr_signal *gnext;   // next signal on global list
	struct thr_signal *gprev;
	struct thr_signal *inext;   // next on interrupt signals
} PACKED_ATT;

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

#define SCHED_HERTZ 200                 // 200 cycles per second

/* The pit will be programmed by setting SCHED_HERTZ.
Pit speed in miliseconds will be:

1/1000 ------ 1 tick every milisecond
1/H --------- x (1/H)/(1/1000) = 1000 / H ticks per millisecond

then the pit should generate an interrupt at 5 ms (with hz set to 200)

5 ms ------- 1 tick 
x ms ------- x / 5 tick.

NOTE: I've checked on bochs by printing tick time and aparently the PIT
is working at around 1000 hz, even though it should be set to 200.

*/

#define TMTICKS(x)        ((UINT32)((UINT32)x) / (1000 / SCHED_HERTZ))
#define ROUND_TIMEOUT(x)  ( (TMTICKS(x) == 0)? 1 : TMTICKS(x) )

/* Init Global signals container */
void init_signals();
/* Init thread signals container */
void init_thr_signals(struct pm_thread *thr);
/* Handle incoming signal messages */
void process_signals();
/* Handle incoming events */
void process_events();
/* Timer handling */
void timer_tick();
/* Init task signals information */
void init_tsk_signals(struct pm_task *tsk);

void wait_signal(struct wait_for_signal_cmd *signal, BYTE blocking, UINT16 task);
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

