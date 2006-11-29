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

#include <os/layout.h>
#include "pmanager_internals.h"
#include "page_list.h"

extern unsigned int ticks;
extern int direction;
extern int running;

/* Timing variables for stealing thread */
unsigned int pa_stealing_stop;
unsigned int pa_stealing_ticksspan;		// how many clock ticks will we wait until we run the thread again
unsigned int pa_stealing_tickscount;	// how many clock ticks will be granted to the thread
unsigned int pa_stealing_tickscurr;		// how many clock ticks have been granted to the thread so far

unsigned int pa_stealing_ticks;			// next scheduled ticks for this thread
int pa_stealing_direction;				// next scheduled ticks direction for this thread

/* Timing variables for aging thread */
unsigned int pa_aging_stop;
unsigned int pa_aging_ticksspan;		// how many clock ticks will we wait until we run the thread again
unsigned int pa_aging_tickscount;		// how many clock ticks will be granted to the thread
unsigned int pa_aging_tickscurr;		// how many clock ticks have been granted to the thread so far

unsigned int pa_aging_ticks;		// next scheduled ticks for this thread
int pa_aging_direction;				// next scheduled ticks direction for this thread

/* Bounds will be give for 1024 pages span, and the difference beween 
	aging thread lower bound and stealing thread upper bound will have to be less than 1/2 of 
	free mem, and greater than 1/16 free mem. */
unsigned int aging_stealing_maxdistance;	// max distance between memory areas for the two processes 
unsigned int aging_stealing_mindistance;	// min distance between memory areas for the two processes
unsigned int aging_stealing_currdistance;	// current distance between memory areas for the two processes

unsigned int pa_en_mem;		// how much free memory is considered to be enough
unsigned int pa_des_mem;	// how much free memory is considered to be the desired ammount
unsigned int pa_min_mem;	// how much free memory is considered to be the minimum at witch threads are given high priority

unsigned int pa_curr_mem;	// how much free memory is available right now

unsigned int min_hits;


/* This will be the actual stealing/aging scheduler function. It'll be run after each thread 
has finished execution, and if necesary, it'll run the threads. */
void pa_schedule()
{
	/* Parameter improvements */
	if(min_hits >= PA_MINHITS && pa_des_mem < pa_en_mem)
	{
		/* Re adjust pa_des_mem */
		pa_des_mem += PA_DESMEMDEC;
		if(pa_des_mem > pa_en_mem) pa_des_mem = pa_en_mem;
	}

	/* Now, how much we have to set free from current region will be calculated by 
		using pa_curr_mem */
	if(pa_curr_mem <= pa_min_mem)
	{
		/* We are starving for memory */
		/* set time spans to a lower value */
		pa_stealing_ticksspan = pa_aging_ticksspan = PA_DEFAULTSPAN / 4;		
		pa_stealing_tickscount = pa_aging_tickscount = PA_DEFAULT_TICKSCOUNT * 4;

		/* set ps_param.sc to a higher value */
		if(pa_stealing_tickscurr == pa_stealing_tickscount)
		{
			/* Try to get pa_des_mem as quick as we can */
			ps_param.sc = (  (((pa_des_mem - pa_curr_mem) / 0x1000) * 4) / PA_REGIONS_COUNT);	
		}
	}
	else if(pa_curr_mem <= pa_des_mem)
	{
		/* We really need memory */
		/* set time spans to a lower value */
		pa_stealing_ticksspan = pa_aging_ticksspan = PA_DEFAULTSPAN / 2;		
		pa_stealing_tickscount = pa_aging_tickscount = PA_DEFAULT_TICKSCOUNT * 2;

		/* set ps_param.sc to a higher value */
		if(pa_stealing_tickscurr == pa_stealing_tickscount)
		{
			/* Try to get pa_des_mem */
			ps_param.sc = (  (((pa_en_mem - pa_curr_mem) / 0x1000) * 2) / PA_REGIONS_COUNT);
		}
	}
	else
	{
		/* ok we don't need much need memory */
		/* set time spans to a lower value */
		pa_stealing_ticksspan = pa_aging_ticksspan = PA_DEFAULTSPAN;		
		pa_stealing_tickscount = pa_aging_tickscount = PA_DEFAULT_TICKSCOUNT;

		/* set ps_param.sc to a higher value */
		if(pa_stealing_tickscurr == pa_stealing_tickscount)
		{
			if(pa_curr_mem == pa_en_mem)
			{
				/* We have too much free memory.. don't waste time fetching more */
				ps_param.sc = 0;	
			}
			else
			{
				/* Try to get pa_en_mem slowly */
				ps_param.sc = (((pa_en_mem - pa_curr_mem) / 0x1000) / PA_REGIONS_COUNT);
				if(ps_param.sc == 0)
				{
					ps_param.sc = 1;
				}
			}
		}
	}

	/* Thread scheduling */

	/* re enable threads stoped if distance is aceptable again */
	if(aging_stealing_currdistance <= aging_stealing_maxdistance || aging_stealing_currdistance >= aging_stealing_mindistance)
	{
		pa_stealing_stop = 0;
		pa_aging_stop = 0;
	}

	/* If run ticks has been reached set pa_aging_tickscurr = 0*/
	if(pa_aging_tickscurr == pa_aging_tickscount && (direction == pa_aging_direction && ticks >= pa_aging_ticks) && !pa_aging_stop)
	{
		pa_aging_tickscurr = 0;	// begin aging thread execution
	}

	/* If run ticks has been reached set pa_stealing_tickscurr = 0*/
	if(pa_stealing_tickscurr == pa_stealing_tickscount && (direction == pa_stealing_direction && ticks >= pa_stealing_ticks) && !pa_stealing_stop)
	{
		pa_stealing_tickscurr = 0;	// begin stealing thread execution
	}

	if(pa_stealing_tickscount > pa_stealing_tickscurr || pa_aging_tickscount > pa_aging_tickscurr)
	{
		/* While thread quantums are not depleted */
		while(pa_stealing_tickscount > pa_stealing_tickscurr || pa_aging_tickscount > pa_aging_tickscurr)
		{
			/* Aging thread */
			/* If thread is being granted it's quantum */
			if( !pa_param.finished && pa_aging_tickscount > pa_aging_tickscurr ) 
			{
				running = PAGEAGING_THR;
				run_thread(PAGEAGING_THR);       /* yielding processor to PAGEAGING_THR thread ... */

				ret_from_sched();
				
				pa_aging_tickscurr++;
			}

			/* stealing thread */
			/* If thread is being granted it's quantum */
			if( !ps_param.finished && pa_stealing_tickscount > pa_stealing_tickscurr ) 
			{
				running = PAGESTEALING_THR;
				run_thread(PAGESTEALING_THR);       /* yielding processor to PAGESTEALING_THR thread ... */

				ret_from_sched();
			}
		}

		/* if threads finished, recalculate bounds */
		if(pa_param.finished && pa_param.pass == 0)
		{
			/* Set next bounds for aging thread */
			aging_stealing_currdistance += PA_REGIONSIZE;
			
			if(pa_param.lb + PA_REGIONSIZE >= FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE)
			{
				pa_param.lb = FIRST_PAGE(PMAN_POOL_PHYS);
				pa_param.ub = PMAN_POOL_PHYS + PA_REGIONSIZE - POOL_TABLES_SIZE;	
			}
			else
			{
				pa_param.lb += PA_REGIONSIZE; 

				if(pa_param.ub + PA_REGIONSIZE >= FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE)
					pa_param.ub = FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE - 0x1000;
				else
					pa_param.ub += PA_REGIONSIZE;
			}

			pa_param.ac = PA_REGIONSIZE / 0x1000;	// FIXME: review values assigned for timing regions, etc by performing a deeper analisis
			pa_param.finished = 0;

			/* If aging thread is far ahead from stealing stop it */
			if(aging_stealing_currdistance > aging_stealing_maxdistance)
			{
				pman_print("aging stopped");
				pa_aging_stop = 1;
			}
		}
		else if(pa_param.finished)
		{
			/* Switch to next pass */
			pa_param.finished = 0;
		}

		if( ps_param.finished )
		{
			aging_stealing_currdistance -= ps_param.last_freed + 0x1000;

			if( (ps_param.lb + ps_param.last_freed + 0x1000) >= FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE)
			{
				ps_param.lb = FIRST_PAGE(PMAN_POOL_PHYS);
				ps_param.ub = PMAN_POOL_PHYS + PA_REGIONSIZE - POOL_TABLES_SIZE;
			}
			else
			{
				ps_param.lb += ps_param.last_freed + 0x1000; 

				if(ps_param.lb + PA_REGIONSIZE >= FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE)
					ps_param.ub = (FIRST_PAGE(PMAN_POOL_PHYS) + POOL_AVAILABLE) - ps_param.lb - 0x1000;
				else
					ps_param.ub = ps_param.lb + PA_REGIONSIZE;
			}

			ps_param.finished = 0;
			ps_param.iterations = 0;

			unsigned int freedmem  = ps_param.fc * 0x1000;

			if(pa_curr_mem <= pa_min_mem && (pa_curr_mem + freedmem > pa_min_mem + PA_MEMPAROLE))
			{
				min_hits = 0;
			}

			pa_curr_mem += freedmem;

			if(aging_stealing_currdistance < aging_stealing_mindistance && !pa_aging_stop)
			{
				pman_print("stealing stopped");
				
				pa_stealing_stop = 1;
			}
		}

		/* Set next execution ticks for threads */
		pa_aging_ticks = ticks + pa_aging_ticksspan;		

		if(pa_aging_ticks < ticks || pa_aging_ticks < pa_aging_ticksspan)
		{
			pa_aging_direction = !direction;
			pa_aging_ticks = pa_aging_ticksspan - (0xFFFFFFFF - ticks);
		}

		pa_stealing_ticks = ticks + pa_stealing_ticksspan;		

		if(pa_stealing_ticks < ticks || pa_stealing_ticks < pa_stealing_ticksspan)
		{
			pa_stealing_direction = !direction;
			pa_stealing_ticks = pa_stealing_ticksspan - (0xFFFFFFFF - ticks);
		}
	}
}




