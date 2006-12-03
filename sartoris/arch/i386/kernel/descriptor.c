/*  
 *   Sartoris microkernel i386 descriptor-handling functions file
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "descriptor.h"
#include "tss.h"

struct seg_desc gdt[GDT_ENT];
struct seg_desc tsk_ldt[MAX_TSK][LDT_ENT];

void init_desc_tables() 
{
	unsigned int i;
	unsigned int l_desc[2];

	/* zero out the gdt */
	for (i=0; i<GDT_ENT; i++) 
	{
		gdt[i].dword0 = 0;
		gdt[i].dword1 = 0;
	}

	/* the dummy descriptor is already zero 
		(the entire gdt is) */

	/* flat kernel code segment */
	gdt[1].dword0 = D_DW0_BASE(0x00000000) | D_DW0_LIMIT(0xffff);
	gdt[1].dword1 = D_DW1_BASE(0x00000000) | D_DW1_LIMIT(0xffff) | 
					DESC_32_BIG | DESC_ER | DESC_DPL(0);
	/* flat kernel data segment */
	gdt[2].dword0 = 0x0000ffff;
	gdt[2].dword1 = 0x00cf9200;
	/* high memory segment */
	gdt[3].dword0 = D_DW0_BASE(0xa0000) | D_DW0_LIMIT(0x60000);
	gdt[3].dword1 = D_DW1_BASE(0xa0000) | D_DW1_LIMIT(0x60000) | 
					DESC_32_SMALL | DESC_RW | DESC_DPL(2);

	/* new gdt load */
	l_desc[0] = ( GDT_ENT * sizeof(struct seg_desc) + ((((unsigned int) gdt) & 0xffff) << 16) );
	l_desc[1] = (unsigned int) gdt >> 16;

	__asm__ ("lgdt %0" : : "m"(*l_desc) );
}

/* size is in bytes, if size is greater than 1Mb then
   it is rounded down to a 4kb multiple                */

void build_ldt(int task_num, void *mem_adr, unsigned int size, int priv) 
{
	unsigned int perms;
	struct seg_desc* ldt;
	unsigned int dw0, dw1;

	if (size > 0x100000) 
	{
		size = size / 0x1000;
		perms = DESC_32_BIG;
	} 
	else 
	{
		perms = DESC_32_SMALL;
	}

	priv++;

	if (priv > 3) { priv = 3; }

	perms |= DESC_DPL(priv);


	ldt = tsk_ldt[task_num];

	dw0 = D_DW0_BASE(mem_adr) + D_DW0_LIMIT(size);
	dw1 = D_DW1_BASE(mem_adr) + D_DW1_LIMIT(size) + perms;

	ldt[0].dword0 = dw0;   /* exec/read */
	ldt[0].dword1 = dw1 + DESC_ER;
	ldt[1].dword0 = dw0;   /* read/write */
	ldt[1].dword1 = dw1 + DESC_RW;
}

/* LDT segment descriptor */
void init_ldt_desc(int task_num, int priv) 
{
	int i;
	unsigned int ldt_adr;
	unsigned int perms;

	priv++;

	if (priv > 3) { priv = 3; }

	perms = DESC_32_SMALL + DESC_LDT + DESC_DPL(priv);

	i = GDT_LDT + task_num;
	ldt_adr = (unsigned int) &tsk_ldt[task_num];
	gdt[i].dword0 = D_DW0_BASE(ldt_adr) + D_DW0_LIMIT(LDT_SIZE);
	gdt[i].dword1 = D_DW1_BASE(ldt_adr) + D_DW1_LIMIT(LDT_SIZE) + perms;
}

void inv_ldt_desc(int task_num) 
{
	int i = GDT_LDT + task_num;

	gdt[i].dword0 = 0;
	gdt[i].dword1 = 0;
}

/* Initialize global TSS descriptor on GDT */
int init_tss_desc() 
{
	int i;
	unsigned int tss_adr;
	unsigned int perms;

	perms = DESC_TSS + DESC_DPL(0);

	tss_adr = (unsigned int) &global_tss;
	gdt[GDT_TSS].dword0 = D_DW0_BASE(tss_adr) + D_DW0_LIMIT(sizeof(struct tss));
	gdt[GDT_TSS].dword1 = D_DW1_BASE(tss_adr) + D_DW1_LIMIT(sizeof(struct tss)) + perms;

	return GDT_TSS;
}

void hook_syscall(int num, int dpl, void *ep, unsigned int nparams) 
{
	gdt[GDT_SYSCALL+num].dword0 = (KRN_CODE << 16) | (((unsigned int)ep) & 0xffff);
	gdt[GDT_SYSCALL+num].dword1 = (((unsigned int)ep) & 0xffff0000) | 0x8c00 | (nparams & 0x1f) | DESC_DPL(dpl);
}
