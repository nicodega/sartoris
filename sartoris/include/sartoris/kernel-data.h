/*  
 *   Sartoris data access header
 *   (included from several files in sartoris/mk)   
 *
 *   Copyright (C) 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */


/* This is static data used by the arch-neutral section of the 
 * kernel. 
 */

/* Do not include this from arch-specific sections, 
 * use exported-data.h for that.
 */

#ifndef _KERNEL_DATA_H_
#define _KERNEL_DATA_H_

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

/*
Based on the following flags, wen we run out of MK memory, 
we will request a page from the OS on top. If this flag
is not DYN_PGLVL_NONE then the given call *must* fail.
*/
#define DYN_PGLVL_NONE     0x0
#define DYN_PGLVL_THR      0x1      // handling a Thread dynamic memory need
#define DYN_PGLVL_TSK      0x2		// handling a Task dynamic memory need
#define DYN_PGLVL_SMO      0x3      // handling an SMO dynamic memory need
#define DYN_PGLVL_PRT      0x4      // handling a Port dynamic memory need
#define DYN_PGLVL_MSG      0x5      // handling a Message dynamic memory need
#define DYN_PGLVL_IDX      0x6      // handling index dynamic memory need

extern int dyn_pg_lvl;
extern int dyn_pg_nest;
extern int dyn_pg_thread;

extern int dyn_pg_ret;
extern void *dyn_pg_ret_addr;

#endif
