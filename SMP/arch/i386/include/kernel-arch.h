
#ifndef _KERNEL_ARCH_H_
#define _KERNEL_ARCH_H_

#include "i386.h"

/* Our state structure size, including stack size */
#ifdef FPU_MMX
#define ARCH_STATE_SIZE (72 + 512 + STACK0_SIZE)
#else
#define ARCH_STATE_SIZE (72 + STACK0_SIZE)
#endif

/* Size needed for per-task information on arch dependant section */
#define ARCH_TASK_SIZE (sizeof(struct i386_task))

/* Boot information and MMAP will be placed next to the mapping */
#define BOOTINFO_SIZE	0x10000		/* This is the maximum size bootinfo and mmap will have (64kb) */
#define BOOTINFO_PHYS	0x200000	/* This is where sartoris loader will copy bootinfo struct and mmap */
#define BOOTINFO_PAGES  0x10

#ifdef __KERNEL__

#ifndef PAGING

	/* memory layout when paging is disabled */
#	define KRN_OFFSET  0x00100000   /* physical */
#	define KRN_SIZE    0x000F0000   /* physical */
#	define USER_OFFSET 0x00200000   /* linear   */
#	define INIT_SIZE   0x00500000   /* physical */
#	define INIT_OFFSET 0x00A00000   /* physical */

#else

	/* memory layout with paging     */
#	define KRN_OFFSET   0x00100000   /* physical */
#	define KRN_SIZE     0x000F0000   /* physical */

	/* 
	When dynamic memory allocation is enabled, 
	sartoris base will be moved so we can map 
	needed dynamic pages.
	*/
#	define USER_OFFSET  MAX_ALLOC_LINEAR   /* linear (112MB of virtual space available for dynamic memory needs)  */

#	define INIT_SIZE    0x00400000   /* physical */ /* last 64kb will contain bootinfo struct and mmap */
#	define INIT_OFFSET  0x00900000   /* physical */
#	define BOOTINFO_MAP (INIT_SIZE - BOOTINFO_SIZE)   /* virtual  */
#	define INIT_PAGES (INIT_SIZE / 0x1000)

	/* 
		NOTE: we will use memory from second MB of the linear address
		up to MAX_THR * PG_SIZE
		for page mappings on intra-task memory transfers. 
		see i386/kernel/cpu.c, #define AUX_MAPPING_BASE 
	*/

#	define PG_SIZE 0x1000
#	define PG_LEVELS 3
#	define IS_PAGE_FAULT(exc_num) ((char)((exc_num) == 14))
#	define PAGE_FAULT_INT   14

/* Kernel linear memory size (for mappings)
NOTE: We will make it a 4MB multiple value, because
right after it, dynamic memory pages will be mapped
and we want them on different page tables.
*/

#if (0x200000 + (MAX_THR * PG_SIZE)) % PG_SIZE == 0
    #define KERN_LMEM_SIZE  (0x200000 + (MAX_THR * PG_SIZE))  /* Size of sartoris Low Memory block (does not include dynamic memory): 1 page per thread + 2MB */
#else
    #define KERN_LMEM_SIZE  (0x200000 + (MAX_THR * PG_SIZE)) + (0x400000 - (0x200000 + (MAX_THR * PG_SIZE)) % 0x400000)
#endif
#define KERN_MAPPINGS_START 0x200000
/* kernel page table(s) for mapping, shared by everybody */
#if (KERN_LMEM_SIZE % 0x400000) == 0
    #define KERN_TABLES (KERN_LMEM_SIZE / 0x400000 )
#else
    #define KERN_TABLES ((KERN_LMEM_SIZE + (0x400000 - (KERN_LMEM_SIZE % 0x400000))) / 0x400000)
#endif
#if (KERN_MAPPINGS_START % 0x400000) == 0
    #define KERN_STA_TABLES (KERN_MAPPINGS_START / 0x400000 )
#else
    #define KERN_STA_TABLES ((KERN_MAPPINGS_START + (0x400000 - (KERN_MAPPINGS_START % 0x400000))) / 0x400000)
#endif

#endif /* PAGING */

#endif /* __KERNEL__ */

#endif /* _KERNEL_ARCH_H_ */

