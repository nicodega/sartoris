/*  
 *   Sartoris i386 dependent header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#ifndef I386
#define I386

#include "sartoris/kernel.h"

#define __align(a) __attribute__ ((aligned (a)))

extern int curr_thread;
extern int curr_task;
extern void *curr_base;

extern struct task tasks[];
extern struct thread threads[];

extern struct page_fault last_page_fault;

unsigned int get_flags(void);

#endif

