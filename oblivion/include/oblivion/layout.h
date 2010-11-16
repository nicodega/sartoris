/*  
 *   Oblivion tasks, threads, memory and initial disk layout header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


/* yes, everything is kind of static */

#ifndef _OBLIVION_H_
#define _OBLIVION_H_

#define PMAN_TASK 0
#define CONS_TASK 1
#define FDC_TASK 2
#define RAMFS_TASK 3
#define DMA_MAN_TASK 4
#define HDC_TASK 5

#define SCHED_THR 1            /* scheduler thread*/
#define EXC_HANDLER_THR 2      /* general exception handler*/
#define CONSM_THR 5            /* console server thread */
#define CONSK_THR 6            /* keyboard handler */
#define FDCM_THR 7             /* floppy server thread */
#define TIMER_THR 8            /* timer handler */
#define FDCI_THR 9             /* floppy int handler */
#define RAMFS_THR 10           /* ram-filesystem thread */
#define DMA_MAN_THR 11         /* dma-manager thread */

#define INIT_CODE_SIZE 0x1000
#define SRV_DISK_SIZE 0x4000

#define SRV_MEM_BASE 0x200000
#define SRV_SLOT_SIZE 0x10000

#define PRC_MEM_BASE 0x300000
#define PRC_SLOT_SIZE 0x10000

#define SRV_PDIR_BASE 0x4f0000
#define SRV_PTAB_BASE 0x5e0000

#define PRC_PDIR_BASE 0x6f0000
#define PRC_PTAB_BASE 0x7e0000

#define PMAN_SLOT 0
#define CONS_SLOT 1
#define FDC_SLOT 2
#define RAMFS_SLOT 3
#define DMA_MAN_SLOT 4
#define HDC_SLOT 5

#define PAGE_SIZE 0x1000

#endif
