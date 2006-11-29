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

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#ifndef PMANINT_SIGNALSH
#define PMANINT_SIGNALSH

#include <services/pmanager/signals.h>

extern unsigned int ticks;	// clock ticks, when it reaches 0xFFFFFFFF it will return to 0 again
extern int direction;	// each time ticks overflows this will be set to !direction

/* A signal for an specified thread */
struct thr_signal
{
	int thread;					// the thread this signal is for
	unsigned char event_type;	// event which will trigger the signal
	unsigned char id;			// identifier
	unsigned short task;		// task from which the signal is expected
	unsigned short signal_param0;
	unsigned short signal_param1;
	unsigned short signal_port;	// port where the signal response must be sent (including errors)

	unsigned int timeout;		// timeout in sched ticks (using ROUND_TIMEOUT(x) to convert from milliseconds) 
								// plus ticks (it could be less because of overflow but that is taken onto account). 
	int dir;					// if timeout + ticks overflows, this will be set to !direction
	int infinite;
	struct thr_signal *tnext;	// next signal on thread list
	struct thr_signal *tprev;
	struct thr_signal *gnext;	// next signal on global list
	struct thr_signal *gprev;
} PACKED_ATT;

/* This structure will hold signals for a given thread */
struct thr_signals
{
	struct thr_signal *blocking_signal;	// there will only be one blocking signal at a given time
	struct thr_signal *first;			// first signal on the thread signals list 
	int next;	// next on the signals container
	int prev;	// next on the signals container
} PACKED_ATT;

/* Global signals container */
struct signals
{
	struct thr_signals tsignals[MAX_THR];	// one signal container for each thread
    int first_thr;							// first thread with waiting signals
	struct thr_signal *first;			// first signal on the global signals list
										// (signals will be ordered by timeout)
} PACKED_ATT;

extern struct signals signals;	// global signals container.

#define SCHED_HERTZ 200		// 200 cycles per second

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

#define TMTICKS(x)		((unsigned int)((unsigned int)x) / (1000 / SCHED_HERTZ))
#define ROUND_TIMEOUT(x)	( (TMTICKS(x) == 0)? 1 : TMTICKS(x) )

void init_signals();
/* Handle incoming signal messages */
void process_signals();
/* Handle incoming events */
void process_events();
/* Timer handling */
void timer_tick();

void wait_signal(struct wait_for_signal_cmd *signal, int blocking, int task);
void discard_signal(struct wait_for_signal_cmd *dsignal, int task);
void event(struct event_cmd *evt, int task);

void send_signal(struct thr_signal *signal, struct event_cmd *evt, int ret);
void insert_signal(struct thr_signal *signal, int thread);
void remove_signal(struct thr_signal *signal, int thread);
int signal_comp(struct thr_signal *s0, struct thr_signal *s1);


#endif

