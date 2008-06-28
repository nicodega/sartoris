
#ifndef _KERNEL_ARCH_H_
#define _KERNEL_ARCH_H_

#include "i386.h"

/* Our state structure size, including stack size */
#ifdef FPU_MMX
#define ARCH_STATE_SIZE (576 + STACK0_SIZE)
#else
#define ARCH_STATE_SIZE (60 + STACK0_SIZE)
#endif
/* Size needed for per-task information on arch dependant section */
#define ARCH_TASK_SIZE (sizeof(struct i386_task))
/* kernel page table(s) for mapping, shared by everybody */
#define KERN_LMEM_SIZE  (0x100000 + (MAX_THR * PG_SIZE))  /* 1 page per thread + 1MB */
#define KERN_TABLES     ((KERN_LMEM_SIZE + (0x400000 - (KERN_LMEM_SIZE % 0x400000))) / 0x400000 )

#ifdef __KERNEL__

#define BOOTINFO_PHYS	0x100000	/* This is where sartoris loader will copy bootinfo struct and mmap */
#define BOOTINFO_SIZE	0x10000		/* This is the maximum size bootinfo and mmap will have (64kb) */
#define BOOTINFO_PAGES  0x10

#ifndef PAGING

	/* memory layout when paging is disabled */
#	define KRN_OFFSET  0x00000000   /* physical */
#	define KRN_SIZE    0x000a0000   /* physical */
#	define USER_OFFSET 0x00100000   /* linear   */
#	define INIT_SIZE   0x00400000   /* physical */
#	define INIT_OFFSET 0x00200000   /* physical */

#else

	/* memory layout with paging     */
#	define KRN_OFFSET   0x00000000   /* physical */
#	define KRN_SIZE     0x000a0000   /* physical */

	/* 
	When dynamic memory allocation is enabled, 
	sartoris base will be moved so we can map 
	needed dynamic pages.
	*/
#	define USER_OFFSET  MAX_ALLOC_LINEAR   /* linear (200MB of virtual space available for dynamic memory needs)  */

#	define INIT_SIZE    0x00400000   /* physical */ // 4MB size (last 64kb will hold bootinfo)
#	define INIT_OFFSET  0x00800000   /* physical */
#	define BOOTINFO_MAP (INIT_SIZE - BOOTINFO_SIZE)   /* virtual  */
#	define INIT_PAGES (INIT_SIZE / 0x1000)

	/* 
		NOTE: we will use memory from first MB of the linear address
		up to MAX_THR * PG_SIZE
		for page mappings on intra-task memory transfers. 
		see i386/kernel/cpu.c, #define AUX_MAPPING_BASE 
	*/

#	define PG_SIZE 0x1000
#	define PG_LEVELS 3
#	define IS_PAGE_FAULT(exc_num) ((char)((exc_num) == 14))
#	define PAGE_FAULT_INT   14

#endif /* PAGING */

#endif /* __KERNEL__ */

#endif /* _KERNEL_ARCH_H_ */

