/*  
 *   Sartoris main kernel header                                          
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#ifndef _CS_H_
#define _CS_H_

#ifdef __KERNEL__

#include <sartoris/cpu-arch.h>

/* This is a lock flag, used by mk_enter/mk_leave */
#ifdef _SMP_
static volatile int mk_lock;
#endif

static inline void spinwait()
{
#ifdef _SMP_
	while(arch_test_and_set(mk_lock, 1)){}
#endif
}

static inline void spinleave()
{
	// set 0 for the spinlock and let them kill each other for it //
#ifdef _SMP_
	mk_lock = 0;
#endif
}

#endif

static inline void mk_cs_init()
{
#ifdef _SMP_
	mk_lock = 0;
#endif	
}

/* This function will Enter a sartoris critical section. 
NOTE: This function is SMP ready. */
static inline int mk_enter()
{
	arch_cli();
#ifdef _SMP_
	spinwait();
#endif
}

/* This function will Leave a sartoris critical section. 
NOTE: This function is SMP ready. */
static inline void mk_leave(int x)
{
	arch_sti(x);
#ifdef _SMP_
	spinleave();
#endif

}

#endif 



