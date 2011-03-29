#ifndef _CPU_ARCH_INL_H
#define _CPU_ARCH_INL_H

#include "kernel-arch.h"

#define HAVE_INL_CLI

extern unsigned int get_flags(void);

static inline int arch_cli(void) 
{   
	unsigned int flags;

	flags = get_flags();  
	if (flags & 0x00000200) 
	{
		__asm__ ("cli" : :);
		return 1;
	} 
	else 
	{
		return 0;
	}
}

#define HAVE_INL_STI

static inline void arch_sti(int x) { if (x) __asm__ ("sti" : :); }

#define HAVE_INL_GET_PAGE_FAULT

static inline void *arch_get_page_fault(void) 
{ 
	void *pf_a; 
	__asm__ ("movl %%cr2, %0" : "=r" (pf_a) : : "cc");
	return pf_a;
} 

#define HAVE_INL_ISSUE_PAGE_FAULT

extern int_14;
static inline void arch_issue_page_fault(void) 
{
    // int$14 won't push the error code!
    // Simulate the interrupt call.
	//__asm__ __volatile__ ("pushl $0; int $14");
    __asm__ __volatile__ (
        "pushf;"
        "cli;"
        "pushl %%cs;"
        "pushl $1f;"
        "pushl $0;"
        "jmp int_14;"
        "1: " : : );

    //__asm__ __volatile__ ("xchg %%ax,%%ax;"::);
}


#ifdef PAGING
static inline int arch_flush_tlb() 
{
	/* touch cr3, processor invalidates all tlb entries */
	__asm__ __volatile__ ("movl %%cr3, %%eax\n\t" 
			 "movl %%eax, %%cr3" :  :  : "cc", "eax" );
}

static inline void invalidate_tlb(void *linear) 
{
	__asm__ __volatile__ ("invlpg %0" : : "m" (linear));

	arch_flush_tlb();   /* FIXME !!! */
}
#endif

#define MAKE_KRN_PTR(x) (void*) ((unsigned int)(x) + (unsigned int)curr_base)
#define MAKE_KRN_SHARED_PTR(t, x) (void*) ((unsigned int)(x) + (unsigned int)(GET_PTR(t,tsk))->mem_adr)
#define SUMOVERFLOW(x, y) (((unsigned int)x+(unsigned int)y) < (unsigned int)x || ((unsigned int)x+(unsigned int)y) < (unsigned int)y)
#endif
