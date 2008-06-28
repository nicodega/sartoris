/*  
 *   Sartoris microkernel i386 tss support
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@gmail.com      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "descriptor.h"
#include "tss.h"
#include "paging.h"
#include "lib/indexing.h"
#include "lib/containers.h"

struct tss global_tss;
struct thr_state *curr_state; // current state loaded

/* Must be issued upon initialization */
void arch_init_global_tss()
{
	int desc = init_tss_desc();

	global_tss.back_link = 0;
	global_tss.ss0 = KRN_DATA_SS;
	global_tss.esp0 = 0;
	global_tss.ss1 = 0;
	global_tss.esp1 = 0;
	global_tss.ss2 = 0;
	global_tss.esp2 = 0;
	global_tss.io_map = 0;
	curr_state = GET_THRSTATE_ARCH(0);    // first state on system will be the bogus task one

	/* 
	Set thr_states[0] sflags to 0, so mmx/fpu state
	for dummy task is not preserved!
	*/
	curr_state->sflags = 0;

	/* Load global task state segment descriptor */
	__asm__ __volatile__ (
	"movw %0, %%ax\n\t"
	/* Shift the selector by 3 */
	"shlw $3, %%ax\n\t"   
	"ltr %%ax"
	:: "m" (desc));
}

void build_tss(struct thr_state *thr_state, int id, int task_num, int priv, void *ep, void *stack) 
{ 
	thr_state->sflags = 0;
	thr_state->eip = (unsigned int)ep;
	if(priv < 3)
		thr_state->ldt_sel = ((GDT_LDT + priv) << 3);
	else
		thr_state->ldt_sel = ((GDT_LDT + 3) << 3);
	thr_state->gs = 0;
	/* esp will be initially our stack 0 */
	thr_state->esp = (unsigned int)((unsigned int)thr_state + sizeof(struct thr_state) + STACK0_SIZE - 4);  
	/* 
	ebp will initially be set to the thread stack, upon
	first preservation of state, it'll be set to current
	stack0 ebp.
	*/
	thr_state->ebp = (unsigned int)stack;

	if (priv==0) 
	{
		thr_state->ldt_sel |= 0x1;
		thr_state->cs = PRIV1_CODE_SS;
		thr_state->ds = PRIV1_DATA_SS;
		thr_state->es = PRIV1_DATA_SS;
		thr_state->fs = PRIV1_HIMEM_SS;
		thr_state->ss = PRIV1_DATA_SS;    
	} 
	else if (priv==1) 
	{
		thr_state->ldt_sel |= 0x2;
		thr_state->cs = PRIV2_CODE_SS;
		thr_state->ds = PRIV2_DATA_SS;
		thr_state->es = PRIV2_DATA_SS;
		thr_state->fs = PRIV2_HIMEM_SS;
		thr_state->ss = PRIV2_DATA_SS;       
	} 
	else 
	{
		thr_state->ldt_sel |= 0x3;
		thr_state->cs = PRIV3_CODE_SS;
		thr_state->ds = PRIV3_DATA_SS;
		thr_state->es = PRIV3_DATA_SS;
		thr_state->fs = 0;
		thr_state->ss = PRIV3_DATA_SS;    
	}
	thr_state->esi = 0;
	thr_state->edi = 0;
	thr_state->ebx = 0;
	
	if (priv < 2) 
		thr_state->eflags = 0x00002002;
	else 
		thr_state->eflags = 0x00002202; /* bits 12,13: iopl=2; 
					                           bit 9: int enable; bit 1: reserved */  
}


