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
		COPIED!! IT WOULD DESTROY IMAGE DATA

		///////////////////////////////////////////////
	*/

	/*
		Sartoris left thigs this way:
			- 0x100000 end of microkernel space (sartoris loader left bootinfo struct right here!)
			- 0x800000 this is were we are running

		What we will do is:

			- Create a page directory on 0x100000 (1 MB boundary)
			- Create a table for the pman process to fit in. (4MB)
			- Create a new task, for the process manager main.
			- Create a new thread for the pman, with ep to pmain_init_stage2().
			- Run the thread.
	*/

	/* Copy multiboot info left by sartoris loader to PMAN_MULTIBOOT_PHYS 
	   NOTE: Sartoris has mapped 4mb of our linear address space. Bootinfo is placed 
	   at PMAN_SARTORIS_MMAP_LINEAR in our space :), but we have to be careful, because that area
	   is mapped at PMAN_SARTORIS_MMAP_PHYS on physical memory.
	   What I'll do is map from PMAN_STAGE1_MAPZONE (it's safe for the init stack is placed at
	   0x3F0000 - 0x4) onto PMAN_MULTIBOOT_PHYS and copy exactly 64kb.
	*/
	
	map_pages(INIT_TASK_NUM
		, PMAN_STAGE1_MAPZONE + SARTORIS_PROCBASE_LINEAR
		, PMAN_MULTIBOOT_PHYS
		, PMAN_MULTIBOOT_PAGES
		, PGATT_WRITE_ENA
		, 2);

	/* Now copy boot info :) */
	dest = (INT32*)PMAN_STAGE1_MAPZONE;
	src = (INT32*)PMAN_SARTORIS_MMAP_LINEAR;

    for(i = 0; i<(PMAN_MULTIBOOT_SIZE / 4);i++){ dest[i] = src[i]; }

	/* Create page dir */
	page_in(PMAN_TASK, (ADDR)0, (ADDR)(PMAN_PDIR_PHYS), 0, 0);

	/* 
	Create a page table 
	PMAN (Code + Data) should fit on 4MB 
	*/
	page_in(PMAN_TASK, (ADDR)SARTORIS_PROCBASE_LINEAR, (ADDR)(PMAN_PTBL_PHYS), 1, PGATT_WRITE_ENA);

	/* Now the pages, starting at 0x100000 + 0x2000 */
	map_pages(PMAN_TASK
		, SARTORIS_PROCBASE_LINEAR
		, PMAN_CODE_PHYS
		, PMAN_CODE_PAGES
		, PGATT_WRITE_ENA
		, 2);

	/* Create the task */
	srv.mem_adr = (ADDR)SARTORIS_PROCBASE_LINEAR;       // virtual
	srv.size    = 0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR; 
	srv.priv_level = 0;			// service
	
	if(create_task(PMAN_TASK, &srv, (ADDR) 0, PMAN_SIZE) < 0) STOP;

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



