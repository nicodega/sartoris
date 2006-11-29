
#include "types.h"
#include "scheduler.h"
#include "io.h"
#include "command.h"
#include "loader.h"
#include "pman_print.h"
#include "interrupts.h"

int pman_stage = PMAN_STAGE_INITIALIZING;

/* Process Manager Main loop */
void process_manager()
{
	BOOL done = FALSE;
	BOOL intraised = FALSE;

	__asm__ ("cli" : :);
	
	/* Initialization 
		- Search For Root Partition on OFS file system.
		- Search For Swap Partition on ATA device
	*/
	pman_stage = PMAN_STAGE_INITIALIZING;
	
	io_begin_init();
	
	while(!done)
	{
		/* Schedule next thread */
		intraised = sch_schedule();
		
		/* Signals and timer */
		process_signals();
		process_events();

		if(intraised) timer_tick();

		/* Process Incoming Commands */
		cmd_process_msg();
			
		done = io_initialized();	
	}

	/* SEND A SIGNAL TO ALL INIT SERVICES 
	TELLING THEM SYSTEM HAS BEEN INITIALIZED
	AND THEY MIGHT NOW CREATE TASKS
	*/

	pman_stage = PMAN_STAGE_RUNING;

	/* Enter the main Loop */
	done = FALSE;

	/* Main Loop */
	while(!done)
	{
		/* Schedule next thread */
		intraised = sch_schedule();

		/* Signals and timer */
		process_signals();
		process_events();

		if(intraised) timer_tick();

		/* Process Incoming Commands */
		cmd_process_msg();
			
		/* Process IO */
		io_process_msg();

		/*if(shuttingDown())
		{
			if(cmd_shutdown_step())
			{
				// Shutdown has just closed all apps //
				done = TRUE;
			}
		}*/
	}

	/* Print "You can now turn off your computer" Message */
	pman_print_and_stop("You can now Safely turn off your computer.");
}
