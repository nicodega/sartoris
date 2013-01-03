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

#include "time.h"

TIME time;

/* Initialize timer ticks and direction. */
void init_timer()
{
	time.ticks = 0;
	time.dir = 0;
}

/* Timer handling */
void timer_tick()
{
	if(time.ticks == 0xFFFFFFFF)
	{
		time.ticks = 0;
		time.dir = !time.dir;
	}
	else
	{
		time.ticks++;
	}
}

/* Add ticks to a */
void time_add_ticks(TIME *a, UINT32 ticks)
{
	if((UINT32)0xFFFFFFFF - a->ticks >= ticks)
	{
		a->ticks += ticks;
	}
	else
	{
		a->dir = !a->dir;
		a->ticks = ticks - ((UINT32)0xFFFFFFFF - a->ticks);
	}
}

void time_add_sec(TIME *t, UINT32 sec)
{
	sec = SEC_TO_TICKS(sec);

	if((UINT32)0xFFFFFFFF - t->ticks >= sec)
	{
		t->ticks += sec;
	}
	else
	{
		t->dir = !t->dir;
		t->ticks = sec - ((UINT32)0xFFFFFFFF - t->ticks);
	}
}
