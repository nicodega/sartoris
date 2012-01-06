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

        /* process sartoris events */
        sch_process_portblocks();

		/* Process Incoming Commands */
		cmd_process_msg();
			
		done = io_initialized();
	}
    
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

        /* process sartoris events */
        sch_process_portblocks();

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
    __asm__ __volatile__ ("cli; hlt;"::);
}
