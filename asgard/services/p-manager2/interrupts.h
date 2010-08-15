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

#ifndef INTERRUPTSH
#define INTERRUPTSH

#include "types.h"
#include <drivers/pit/pit.h>
#include "task_thread.h"

#define MAX_INTERRUPT 64
#define IA32FIRST_INT 30

void int_init();
void gen_ex_handler();
/* decide if we were run by the int or by a reschedule
 IMPORTANT: ints must be disabled when using this. */
BOOL is_int0_active();
BOOL int_can_attach(struct pm_thread *thr, UINT32 interrupt);
BOOL int_attach(struct pm_thread *thr, UINT32 interrupt, int priority);
BOOL int_dettach(struct pm_thread *thr);

UINT32 int_clear();
void int_set(UINT32 x);

struct interrupt_signals_container
{
	struct thr_signal *first;
	UINT32 total;
} PACKED_ATT;

BOOL int_signal(struct pm_thread *thread, struct thr_signal *signal, INT32 interrupt);


#endif
