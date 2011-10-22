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

/* 
Signaling on pmanager will allow a given thread to sleep 
until a signal arrives. (this will be extended when stdsignals is
implemented)

NOTE: A given thread won't be allowed to wait for the same signal, coming
from the same task twice. When a signal times out, or an event arrives 
the signal queue will be checked, and if a new WAIT_FOR_SIGNAL event is 
present, with same task, event_type and thread, that signal
will override the current signal.
*/

#ifndef PMANSIGNALSH
#define PMANSIGNALSH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#define PMAN_SIGNALS_PORT	3		// port where pman will listen for WAIT_FOR_SIGNAL
#define PMAN_EVENTS_PORT	4		// port where pman will listen for EVENTs

#define WAIT_FOR_SIGNAL	        0x1     // a thread will send this msg when it wants to sleep until a given signal arrives
#define WAIT_FOR_SIGNAL_NBLOCK	0x2     // a thread will send this msg when it wants to wait for a signal, but it wants to keep running
#define EVENT			        0x3     // an event is sent by threads and it will trigger signals
#define SIGNAL			        0x4     // a signal has ocurred
#define DISCARD_SIGNAL	        0x5     // tell the process manager to dicard a given signal
#define SET_SIGNAL_HANDLER	    0x6     // Set a task wide exceptions signal handler.
#define SET_SIGNAL_STACK	    0x7     // Set a stack for the thread signal handler.

/* Errors */
#define SIGNAL_OK		0x0
#define SIGNAL_TIMEOUT	0x1
#define SIGNAL_FAILED	0x2

/* Inifnite value for signal timeout */
#define PMAN_SIGNAL_TIMEOUT_INFINITE	0
#define PMAN_SIGNAL_REPEATING			0xFFFFFFFF		// a repeating signal wont timeout and wont be discarded until discard message is sent
														// or task is removed

/* Define to ignore a param of an event (i.e. generate the signal ignoring the param) */
#define PMAN_SIGNAL_PARAM_IGNORE	0xFFFFFFFF

/* Pman special signals (when task is set to PMAN_TASK) */
#define PMAN_SLEEP			0x1		// this will just wait for timeout (timeout sent cannot be infinite)
#define PMAN_INTR			0x2		// this signal will be sent when an interrupt is raised.
#define PMAN_EXCEPTION		0x3		// This signal will be sent to a task when a thread provokes an exception

#define PMAN_GLOBAL_EVENT	((unsigned short)0xFFFF)	// if this value is set to the task identifier of an event
													    // all threads with a pending signal for this event
													    // will be signaled.

struct wait_for_signal_cmd
{
	unsigned char command;		// set to WAIT_FOR_SIGNAL
	unsigned short thr_id;		// a thread identifier
	unsigned char event_type;	// event which will trigger the signal
	unsigned char id;	        // an identifier number (can be set to anything and it will be returned as is)
	unsigned short task;		// task from which the signal is expected (can be the same task)
	unsigned int timeout;		// timeout in milliseconds. 
	unsigned int signal_param;
	unsigned char signal_port;	// port where the signal response must be sent (including errors)
} PACKED_ATT;

struct discard_signal_cmd
{
	unsigned char command;		// set to DISCARD_SIGNAL
	unsigned short thr_id;		// a thread identifier
	unsigned char event_type;	// event which will trigger the signal
	unsigned char id;	        // an identifier number (can be set to anything and it will be returned as is)
	unsigned short task;		// task from which the signal is expected (can be the same task)
	unsigned int padding;		 
	unsigned int signal_param;
	unsigned char signal_port;	// port where the signal response must be sent (including errors)
} PACKED_ATT;

struct set_signal_handler_cmd
{
	unsigned char command;		// set to SET_SIGNAL_HANDLER
	unsigned short exceptions_port;
    unsigned char ret_port;
	unsigned int padding;
	void *handler_ep;           // the entry point of the signals handler
	unsigned int padding1;
} PACKED_ATT;

struct set_signal_handler_res
{
	unsigned char command;		// set to SET_SIGNAL_HANDLER
	unsigned short padding;
    unsigned char result;
	unsigned int padding[3];
} PACKED_ATT;

struct set_signal_stack_cmd
{
	unsigned char command;		// set to SET_SIGNAL_STACK
	unsigned short thr_id;
    unsigned char ret_port;
	unsigned int padding;
	unsigned int size;          // stack size in pages
	void *stack;                // the stack address for the signals handler. If NULL, it will use the same stack the thread is using.    
} PACKED_ATT;

struct set_signal_stack_res
{
	unsigned char command;		// set to SET_SIGNAL_STACK
	unsigned short thr_id;
    unsigned char result;
	unsigned int padding[3];
} PACKED_ATT;

/*
This is the message sent to a task when a signal occurs.
*/
struct signal_cmd
{
	unsigned char command;		// set to SIGNAL
	unsigned short thr_id;		// a thread identifier (same as in WAIT_FOR_SIGNAL)
	unsigned char event_type;	// event which triggered the signal (the one that woke up the thread)
	unsigned char id;
	unsigned short task;		// task who originated the signal
	void *eaddr;                // this will be used only on pman exceptions, and will contain the address at whitch the exception raised.
	unsigned int res;
	unsigned char ret;			// if signal timed out it will hold SIGNAL_TIMEOUT. else SIGNAL_OK
} PACKED_ATT;

/*
This is the message a task/thread must send to trigger a signal.
*/
struct event_cmd
{
	unsigned char  command;		// set to EVENT
	unsigned short padding0;	
	unsigned short event_type;	// event type
	unsigned int   param;
	unsigned int   event_res;
	unsigned short task;		// if task is not -1, this event will only apply for a given task.
								// if set to -1 this will be considered a global event, affecting
								// all tasks.
	unsigned char padding1;
} PACKED_ATT;




#endif
