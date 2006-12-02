/*  
 *   Sartoris data access header
 *   (included from several files in sartoris/mk)   
 *
 *   Copyright (C) 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


/* This is static data used by the arch-neutral section of the 
 * kernel. 
 */

/* Do not include this from arch-specific sections, 
 * use exported-data.h for that.
 */

#ifndef _KERNEL_DATA_H_
#define _KERNEL_DATA_H_

extern struct task tasks[MAX_TSK];
extern struct thread threads[MAX_THR];
extern struct smo_container smoc;
extern struct msg_container msgc;

extern int open_ports[MAX_TSK][MAX_TSK_OPEN_PORTS];

extern unsigned int run_perms[MAX_THR][BITMAP_SIZE(MAX_THR)];

extern int int_handlers[MAX_IRQ];
extern unsigned char int_nesting[MAX_IRQ];
extern unsigned char int_active[MAX_THR];

extern int int_stack[MAX_NESTED_INT];
extern int int_stack_pointer;

extern int last_int;

extern struct page_fault last_page_fault;

extern int curr_thread;
extern int curr_task;
extern void *curr_base;
extern int curr_priv;
#endif
