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
Process manager init function.
*/
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "layout.h"
#include "formats/bootinfo.h"
#include "types.h"
#include "mem_layout.h"
#include "helpers.h"
#include "task_thread.h"

#include <drivers/screen/screen.h>

void pman_init_stage2();

void pman_init()
{
	struct task srv;
	struct thread thr;
	UINT32 i;
	INT32 *dest, *src;

	/* Clear Interrupts */
	__asm__ ("cli" : :) ;
        
	/*
		//////////////// IMPORTANT NOTE ///////////////

		DO NOT UNDER ANY CIRCUNSTANCE PRINT OR ACCESS
		GLOBAL VARIABLES UNTIL INIT IMAGE HAS BEEN 
		COPIED!! IT WOULD DESTROY IMAGE DATA BECAUSE
        WE ARE RUNNING ON THE IMAGE ITSELF

		///////////////////////////////////////////////
	*/

	/*
		Sartoris left thigs this way (physical memory):
			- PMAN_SARTORIS_MMAP_PHYS end of microkernel space (sartoris loader left bootinfo struct right here!)
			- PMAN_SARTORIS_INIT_PHYS this is were we are running

		What we will do is:

			- Move bootinfo from PMAN_SARTORIS_MMAP_PHYS to PMAN_MULTIBOOT_PHYS
			- Create a page directory on PMAN_PDIR_PHYS (1 MB boundary)
			- Create a table for the pman process to fit in. (4MB)
			- Create a new task, for the process manager.
			- Create a new thread for pman, with ep to pman_init_step2().
			- Run the thread.
	*/

	/* Copy multiboot info left by sartoris loader to PMAN_MULTIBOOT_PHYS 
	   NOTE: Sartoris has mapped 4mb of our linear address space. Bootinfo is placed 
	   at PMAN_SARTORIS_MMAP_LINEAR in our space :), but we have to be careful, because that area
	   is mapped at PMAN_SARTORIS_MMAP_PHYS on physical memory.
	   What I'll do is map from PMAN_STAGE1_MAPZONE (it's safe to place it at 0x390000 for the init stack is placed at
	   0xCBF0000 - 0x4) onto PMAN_MULTIBOOT_PHYS and copy exactly 64kb.
	*/
	
	map_pages(INIT_TASK_NUM
		, PMAN_STAGE1_MAPZONE + SARTORIS_PROCBASE_LINEAR
		, PMAN_MULTIBOOT_PHYS
		, PMAN_MULTIBOOT_PAGES
		, PGATT_WRITE_ENA
		, 2);

	/* Now copy boot info and MMAP :) */
	dest = (INT32*)PMAN_STAGE1_MAPZONE;
	src = (INT32*)PMAN_SARTORIS_MMAP_LINEAR;

    for(i = 0; i<(PMAN_MULTIBOOT_SIZE / 4);i++){ dest[i] = src[i]; }
		                          // service
	
    srv.mem_adr = (ADDR)SARTORIS_PROCBASE_LINEAR;         // virtual
	srv.size    = 0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR;  // the whole memory is available to PMAN
	srv.priv_level = 0;	
        
	if(create_task(PMAN_TASK, &srv) < 0) STOP;
   
	/* Create page dir */
	page_in(PMAN_TASK, (ADDR)0, (ADDR)(PMAN_PDIR_PHYS), 0, 0);

	/* 
	Create a page table 
	PMAN (Code + Data) should fit on 4MB 
	*/
	page_in(PMAN_TASK, (ADDR)SARTORIS_PROCBASE_LINEAR, (ADDR)(PMAN_PTBL_PHYS), 1, PGATT_WRITE_ENA);

	/* Now map the pages, starting at PMAN_CODE_PHYS */
	map_pages(PMAN_TASK
		, SARTORIS_PROCBASE_LINEAR
		, PMAN_CODE_PHYS
		, PMAN_CODE_PAGES
		, PGATT_WRITE_ENA
		, 2);

	/* Create the task */
	
	if(init_task(PMAN_TASK, (ADDR) 0, PMAN_SIZE)) STOP;   // Initialize the new task with PMAN
                                                          // IMPORTANT: If PMAN size was bigger than 2MB, init would
                                                          // step over MULTIBOOT INFO structure.
   
	/* Create the thread */
	thr.task_num = PMAN_TASK;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 3;
	thr.ep = pman_init_stage2;
	thr.stack = (ADDR)STACK_ADDR(PMAN_STACK_ADDR);

	if(create_thread(SCHED_THR, &thr) < 0) STOP;
    
	/* Run PMAN thread */
	run_thread(SCHED_THR);
	STOP;
}



