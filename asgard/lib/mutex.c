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

#include <sartoris/syscall.h>
#include <lib/scheduler.h>
#include <lib/mutex.h>


void init_mutex(struct mutex *mon)
{
	mon->lock = 1;

	mon->available_turn = 0;
	mon->locking_turn = 0;
	mon->locking_thread = -1;
	mon->recursion = 0;
    mon->lock = 0;
}

#define CMP_AND_SWP(ptr,oldv,newv) (__sync_bool_compare_and_swap (ptr, oldv, newv))

void wait_mutex(struct mutex *mon)
{
	int currthread = get_current_thread();
	unsigned int turn;

    while(!CMP_AND_SWP(&mon->lock,0,1)){ reschedule(); }

	if(mon->locking_thread == currthread)
	{
		mon->recursion++;
		mon->lock = 0;
		return;
	}

	if(mon->locking_thread == -1)
	{
		// ok we can lock the mutex
		mon->locking_thread = currthread;
		mon->locking_turn = mon->available_turn = 0;
		mon->recursion = 0;
		mon->lock = 0;
		return;	
	}

	// mutex is locked or waiting, get a turn and wait until mutex is released
	mon->available_turn = ((mon->available_turn + 1) & TURN_MASK);

	turn = mon->available_turn;	

	mon->lock = 0;

	while(turn != mon->locking_turn){ reschedule(); }

	// our turn has come 
	mon->locking_thread = currthread;
	mon->recursion = 0;
}

void leave_mutex(struct mutex *mon)
{
	int currthread = get_current_thread();

	while(!CMP_AND_SWP(&mon->lock,0,1)){ reschedule(); }

	// check this thread has the mutex
	if(mon->locking_thread != currthread)
	{
		mon->lock = 0;
		return;
	}

	if(mon->locking_thread == currthread && mon->recursion > 1)
	{
		mon->recursion--;
		mon->lock = 0;
		return;
	}

	// see if someone took a turn
	if(mon->available_turn != mon->locking_turn)
	{
		// grant mutex to next turn
        mon->locking_turn = ((mon->locking_turn + 1) & TURN_MASK);

		mon->locking_thread = -2; // intermediate state, indicating the mutex is waiting for next thread
		mon->recursion = 0;
		mon->lock = 0;
		return;
	}
	
	// ok nobody has next turn, reset mutex for safety 
	mon->locking_turn = mon->available_turn = 0;
	mon->locking_thread = -1;
	mon->recursion = 0;
	mon->lock = 0;
}


void close_mutex(struct mutex *mon)
{
	// nothing to be done here
}

int test_mutex(struct mutex *mon)
{
	return mon->locking_thread != -1;
}

int own_mutex(struct mutex *mon)
{
	// check this thread has the mutex
	return (mon->locking_thread == get_current_thread());	
}

