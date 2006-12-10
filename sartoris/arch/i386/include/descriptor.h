#ifndef __DESCRIPTORS_H
#define __DESCRIPTORS_H

#include "selector.h"

struct i386_task;

/* system tables macros */
#define GDT_ENT (SYS_DESC + MAX_SCA + GDT_LDTS + 1)

#define LDT_ENT 3 		
#define LDT_SIZE (LDT_ENT * sizeof(struct seg_desc))

#define GDT_LDTS      4         /* one for each task privilege level
								   upon thread switch, ldt descriptor will 
								   be set to the proper address */
#define SYS_DESC      4        	/* dummy, code, data, himem */

#define GDT_SYSCALL     SYS_DESC
#define GDT_LDT         (GDT_SYSCALL + MAX_SCA)
#define GDT_TSS         (GDT_LDT + GDT_LDTS)

/* segment descriptor macros */
#define DESC_32_BIG   0x00c08000
#define DESC_32_SMALL 0x00408000

#define DESC_TSS 0x00008900 // S = 0 TYPE=TSSD P = 1 G=0 

#define DESC_DPL(x) (((unsigned int)x & 0x3) << 13 )

#define DESC_R        0x00001000
#define DESC_RW       0x00001200
#define DESC_ER       0x00001a00

#define DESC_LDT      0x00000200
#define DESC_CALLG    0x00000c00

#define TASK_GATE    0x00008500
#define TRAP_GATE    0x00008700
#define IRQ_GATE_32  0x00008600

#define D_DW0_BASE(x)  (((unsigned int)x & 0xffff) << 16)
#define D_DW1_BASE(x)  ((unsigned int)x & 0xff000000) + (((unsigned int)x & 0x00ff0000) >> 16)

#define D_DW0_LIMIT(x) ((unsigned int)x & 0xffff)
#define D_DW1_LIMIT(x) ((unsigned int)x & 0x000f0000)

#define G_DW0_OFFSET(x) ((unsigned int)x & 0xffff)
#define G_DW1_OFFSET(x) ((unsigned int)x << 16)

#define G_DW0_SELECTOR(x) ((unsigned int)x << 16)

struct seg_desc 
{
	int dword0;
	int dword1;
};


void init_desc_tables();
int init_tss_desc();
void hook_syscall(int num, int dpl, void *ep, unsigned int nparams);
void build_ldt(struct i386_task *tinf, int task_num, void *mem_adr, unsigned int size, int type);
void switch_ldt_desc(struct i386_task *tinf, int type);
void inv_ldt_desc(int task_num);

#endif
