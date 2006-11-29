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

#include <services/pmanager/signals.h>

#ifndef SIGNALSH
#define SIGNALSH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#include <lib/structures/list.h>

typedef CPOSITION SIGNALHANDLER;

struct signal_response
{
	int thr;	// waiting thread
	int task;	// task from whom the message must be received
	unsigned char event_type;
	unsigned char id;
	unsigned short param1;
	unsigned short param2;
	unsigned short res0;
	unsigned short res1;
	unsigned short res;
	int received;	// initialized to zero, if message arrives, it'll be set to 1
} PACKED_ATT;

#define SIGNALS_PORT	5

/* Inifnite value for signal timeout */
#define SIGNAL_TIMEOUT_INFINITE		PMAN_SIGNAL_TIMEOUT_INFINITE

/* Define to ignore a param of an event (i.e. generate the signal ignoring the param) */
#define SIGNAL_PARAM_IGNORE		PMAN_SIGNAL_PARAM_IGNORE

void sleep(unsigned int milliseconds);
int wait_signal(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned short param1, unsigned short param2, unsigned short *res0, unsigned short *res1);
SIGNALHANDLER wait_signal_async(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned short param1, unsigned short param2, unsigned short *res0, unsigned short *res1);
int check_signal(SIGNALHANDLER sigh, unsigned short *res0, unsigned short *res1);
void discard_signal(SIGNALHANDLER sigh);

void send_event(unsigned short task, unsigned short event_type, unsigned short param1, unsigned short param2, unsigned short res0, unsigned short res1);

#endif
