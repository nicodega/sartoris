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


//#define PRINTDEBUG
#define PRINTDEBUGEXCE
// remove this include
#include <drivers/screen/screen.h>

#include <lib/sprintf.h>

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>

/* TEMPORARY */
#include <os/layout.h>
/* TEMPORARY */

#include <services/pmanager/services.h>

#include <services/stds/stdfss.h>

#include <drivers/pit/pit.h>
#include <lib/reboot.h>
#include <lib/int.h>

#include "pmanager_internals.h"
#include "page_list.h"

char txt_err_smo[] = "\nprocess manager -> error while loading program.";

char txt_server_error[] = "\n\nprocess manager -> server thread xx has died (!!)\n\n";

struct pm_task   task_info[MAX_TSK];
struct pm_thread thread_info[MAX_THR];

unsigned int debug[10] = {0xfefefefe };

page_list free_pages;

int running;
int fsinitialized, fsinitfailed;

/* This will hold physical memory detected upon startup */
unsigned int physical_memory = PMAN_DEFAULT_MEM_SIZE;	// total memory in bytes
unsigned int pool_megabytes = (PMAN_DEFAULT_MEM_SIZE / 0x100000) - ((PMAN_DEFAULT_MEM_SIZE / 0x100000) % 4) - 4; // pool memory in MB (4MB multiple)

/* decide if we were run by the int or by a reschedule
 IMPORTANT: ints must be disabled when using this. */
int is_int0_active()
{
	int res = 0;
	// 0xB = 1011 meaning we will read the ISR (In Service Reg) from the first PIC
	__asm__ __volatile__ ("xor %%eax,%%eax;movb $0x0B, %%al;outb %%al, $0x20;inb $0x20, %%al;andl $0x1, %%eax;movl %%eax, %0" : "=r" (res) :: "eax");
	return res;
}

void ret_from_sched()
{
	/* Next time we are runned either by an int or runthread, 
				we will start here! */
	int intraised = is_int0_active();

	if(intraised) ack_int_master();    /* ok, we're back. ack timer interrupt!*/

	/* Set the thread as waiting only if it was running */
	if (thread_info[running].state == THR_RUNNING)
				thread_info[running].state = THR_WAITING;

	/* Signals and timer */
	process_signals();
	process_events();
	if(intraised) timer_tick();
}

void process_manager(void) 
{
	//struct fs_response fs_res;

	int task_id;
	int id;
	int i, j, port;
	unsigned int linear, physical;

	__asm__ ("cli" : :);

	fsinitialized = 0;
	fsinitfailed = 0;

	/* get a reasonable scheduling frequence */
	adjust_pit(SCHED_HERTZ);

	/* acknowledge first interrupt */

	ack_int_master();

	/* Begin page allocation initialization */
	pa_init();
	int pa_init_ret = 0;

	/* loop forever */

	for(;;) {

		/* this is the scheduler loop: iterate over all threads,
			and yield the processor to the ones that are waiting */

		for (i=0; i<MAX_THR; i++) 
		{
			/* Page allocation schedulling */
			if(pa_init_ret == 1)
			{
				//pa_schedule();
			}

			if(thread_info[i].state == THR_WAITING && task_info[thread_info[i].task_id].state != TSK_KILLED) 
			{
				thread_info[i].state = THR_RUNNING;
				running = i;
				run_thread(i);       /* yielding processor to thread i...   */

				/* 
				NOTE: When we come back from an interrupt, it may be because 
				someone issued run_thread on us. This was fine, until signals
				where introduced, because now, we need to know whether the int
				was raised or run_thread was issued, in order to check timeouts.
				*/
				ret_from_sched();
			}
		}


		/* get requests to process */
		while (get_msg_count(PMAN_COMMAND_PORT) > 0) 
		{
			struct pm_msg_generic msg_generic;

			if ( get_msg(PMAN_COMMAND_PORT, &msg_generic, &task_id) == SUCCESS ) 
			{
				pm_process_command(&msg_generic, task_id);
			}
		}

		if(pa_init_ret == 0)
		{
			pa_init_ret = pa_init_process_msg();

			if(pa_init_ret != 0)
			{
				/* 
					Finished page allocation initialization.
					Begin fs initialization. 
				*/
				//if(pa_init_ret == -1) pman_print("Swap partition not found.");
				
				fsinit_begin();
			}
			continue;
		}
		else if(pa_init_ret == 1)
		{
			pa_check_swap_msg();
		}

		/* process acknowledges from filesystem */
		if(fsinitialized || fsinitfailed)
		{
			pm_process_file_opens();

			pm_process_elf_res();

			pm_process_seeks();

			pm_process_reads();

			pm_process_closes();
		}
		else if(pa_init_ret != 0)
		{
			/* FS is being initialized */
			fsinit_process_msg();
		}
	}
}
