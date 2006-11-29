/*  
*   Oblivion tasks and threads layout header
*   
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
This used to be a static include. Now it should be only a pman include.
Information on tasks and main thread can be configured on initfs2conf file
under pmanager 2 tree.
*/


#ifndef _PMANLAYOUT_H_
#define _PMANLAYOUT_H_

#define O_PAGING


#define PMAN_TASK 	0

/* Pman threads */
#define SCHED_THR			1		/* scheduler thread          */
#define EXC_HANDLER_THR		2		/* general exception handler */
#define PAGEAGING_THR		3		/* page aging pman thread    */
#define PAGESTEALING_THR	4		/* page stealing pman thread */
#define PGF_HANDLER_THR		5		/* page fault handler        */
#define INT_HANDLER_THR		6		/* general interrupt handler */

#endif

