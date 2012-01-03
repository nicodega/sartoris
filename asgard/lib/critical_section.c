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
#include <lib/int.h>
#include <lib/scheduler.h>
#include <lib/critical_section.h>


void init_mutex(struct mutex *mon)
{
	int x = enter_block();

	mon->available_turn = 0;
	mon->locking_turn = 0;
	mon->locking_thread = -1;
	mon->locked = UNLOCKED;
	mon->recursion = 0;

	exit_block(x);
}



void wait_mutex(struct mutex *mon)
{
	int currthread = get_current_thread();
	unsigned int turn;
	int x = enter_block();

	if(mon->locking_thread == currthread && mon->locked == LOCKED)
	{
		mon->recursion++;
		exit_block(x);
		return;
	}

	if(mon->locked == UNLOCKED)
	{
		// ok we can lock the mutex
		mon->locking_thread = currthread;
		mon->locked = LOCKED;
		mon->locking_turn = mon->available_turn = 0;
		mon->recursion = 0;
	
		exit_block(x);
		return;	
	}

	// mutex is locked or waiting, get a turn and wait until mutex is released
	mon->available_turn++;
	if(mon->available_turn == MAXTURN) mon->available_turn = 0;

	turn = mon->available_turn;	

	exit_block(x);

	while(turn != mon->locking_turn){ reschedule(); }

	// our turn has come 
	mon->locking_thread = currthread;
	mon->locked = LOCKED;
	mon->recursion = 0;
}

void leave_mutex(struct mutex *mon)
{
	int currthread = get_current_thread();

	int x = enter_block();

	// check this thread has the mutex
	if(mon->locking_thread != currthread || mon->locked != LOCKED)
	{
		exit_block(x);
		return;
	}

	if(mon->locking_thread == currthread && mon->locked == LOCKED && mon->recursion > 1)
	{
		mon->recursion--;
		exit_block(x);
		return;
	}

	// see if someone took a turn
	if(mon->available_turn != mon->locking_turn)
	{
		// grant mutex to next turn
		mon->locking_turn++;	
		if(mon->locking_turn == MAXTURN) 
			mon->locking_turn = 0;

		mon->locked = WAITING; // intermediate state, indicating the mutex is waiting for next thread
		mon->recursion = 0;
		exit_block(x);
		return;
	}
	
	// ok nobody has next turn, reset mutex for safety 
	mon->locking_turn = mon->available_turn = 0;
	mon->locked = UNLOCKED;
	mon->locking_thread = -1;
	mon->recursion = 0;

	exit_block(x);
}


void close_mutex(struct mutex *mon)
{
	// nothing to be done here
}


int test_mutex(struct mutex *mon)
{
	return mon->locked != UNLOCKED;
}

int own_mutex(struct mutex *mon)
{
	int currthread = get_current_thread();

	int x = enter_block();

	// check this thread has the mutex
	if(mon->locking_thread == currthread && mon->locked == LOCKED)
	{
		exit_block(x);
		return 1;
	}

	exit_block(x);

	return 0;
}

