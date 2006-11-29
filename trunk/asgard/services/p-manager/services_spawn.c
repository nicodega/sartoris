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


#include <sartoris/kernel.h>
#include "pmanager_internals.h"
#include <os/layout.h>
/*
	Here we will create initial system services.
*/
extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

unsigned int pm_spawn_services()
{
	unsigned int allocated = 0;

	/* create DMA manager service  */
	allocated = create_service(DMA_MAN_TASK, DMA_MAN_THR, 0, DMA_MAN_IMG, PM_TASK_SIZE, PMAN_LOWMEM_PHYS, 1);  

	unsigned int imsg[4];
	imsg[0] = PMAN_LOWMEM_PHYS;
	send_msg(DMA_MAN_TASK, 0, imsg);

	/* create console service */
	create_service(CONS_TASK, CONSM_THR, 0, CONS_IMG, PM_TASK_SIZE, 0 , 1);

	/* create directory service */
	create_service(DIRECTORY_TASK, DIRECTORY_THR, 0, DIRECTORY_IMG, PM_TASK_SIZE,0,1);

	/* create fdc service */
	create_service(FDC_TASK, FDCM_THR, 0, FDC_IMG, PM_TASK_SIZE,0,1); 

	/* create shell service */
	create_service(SHELL_TASK, SHELL_THR, 0, SHELL_IMG, PM_TASK_SIZE,0,1);

	/* create ata controler service */
	create_service(ATAC_TASK, ATAC_THR, 0, ATAC_IMG, PM_TASK_SIZE,0,1); 

	/* create ofss service */
	create_service(OFSS_TASK, OFSS_THR , 0, OFSS_IMG, PM_TASK_SIZE,0,1); 

	/* create pipes service */
	create_service(PIPES_TASK, PIPES_THR, 0, PIPES_IMG, PM_TASK_SIZE,0,1);

	return allocated;
}



