#ifndef _KERNEL_ARCH_H_
#define _KERNEL_ARCH_H_


/* configuration for i386 platform */

#define PAGING
#define COMPAT_REL53

/* end of configuration options */


#ifdef __KERNEL__

#define BOOTINFO_PHYS	0x100000	/* This is where sartoris loader will copy bootinfo struct and mmap */
#define BOOTINFO_SIZE	0x10000		/* This is the maximum size bootinfo and mmap will have (64kb) */
#define BOOTINFO_PAGES  0x10

#ifndef PAGING
/* memory layout when paging is disabled */

#define KRN_OFFSET  0x00000000   /* physical */
#define KRN_SIZE    0x000a0000   /* physical */
#define USER_OFFSET 0x00100000   /* linear   */
#define INIT_SIZE   0x00400000   /* physical */
#define INIT_OFFSET 0x00200000   /* physical */

#else
/* memory layout with paging     */

#define KRN_OFFSET   0x00000000   /* physical */
#define KRN_SIZE     0x000a0000   /* physical */
#define USER_OFFSET  0x00800000   /* virtual  */
#define INIT_SIZE    0x00400000   /* physical */ // 4MB size (last 64kb will hold bootinfo)
#define INIT_OFFSET  0x00800000   /* physical */
#define BOOTINFO_MAP (INIT_SIZE - BOOTINFO_SIZE)   /* virtual  */
#define INIT_PAGES (INIT_SIZE / 0x1000)

/* NOTE: the third megabyte of the linear address
   is currently used for the mappings used in
   intra-task memory transfers. 
   see i386/kernel/cpu.c, #define AUX_MAPPING_BASE */

#define PG_SIZE 0x1000
#define PG_LEVELS 3
#define IS_PAGE_FAULT(exc_num) ((char)((exc_num) == 14)) 

#endif /* PAGING */

#endif /* __KERNEL__ */


#endif /* _KERNEL_ARCH_H_ */

