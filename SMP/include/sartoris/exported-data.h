/*  
 *   Sartoris exported data structures functions header
 *   (this file might be included in the arch-dependent section)   
 *
 *   Copyright (C) 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */


/* This is data exported by the arch-neutral portion of
 * the kernel.
 *
 * 
 * IMPORTANT: The arch-dependant section should access this in 
 * a read-only fashion! 
 *
 */

#ifndef __EXPORTED_DATA_H
#define __EXPORTED_DATA_H

extern int curr_thread;
extern int curr_task;
extern void *curr_base;

extern struct task tasks[];
extern struct thread threads[];

#endif
