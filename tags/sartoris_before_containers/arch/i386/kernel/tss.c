/*  
 *   Sartoris microkernel i386 tss support
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
#include "paging.h"

struct thr_state thr_states[MAX_THR];
unsigned char stacks[MAX_THR][STACK0_SIZE];
extern pd_entry *tsk_pdb[MAX_TSK];
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
	curr_state = thr_states;    // first state on system will be the bogus task one

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

void build_tss(int id, int task_num, int priv, void *ep, void *stack) 
{ 
	thr_states[id].sflags = 0;
	thr_states[id].eip = (unsigned int)ep;
	thr_states[id].ldt_sel = ((GDT_LDT + task_num) << 3);
	thr_states[id].gs = 0;
	/* esp will be initially, those on our stack 0 */
	thr_states[id].esp = (unsigned int)(&stacks[id][0]) + STACK0_SIZE - 4;  
	/* 
	ebp will initially be set to the thread stack, upon
	first preservation of state, i'll be set to current
	stack0 ebp.
	*/
	thr_states[id].ebp = (unsigned int)stack;

	if (priv==0) 
	{
		thr_states[id].ldt_sel |= 0x1;
		thr_states[id].cs = PRIV1_CODE_SS;
		thr_states[id].ds = PRIV1_DATA_SS;
		thr_states[id].es = PRIV1_DATA_SS;
		thr_states[id].fs = PRIV1_HIMEM_SS;
		thr_states[id].ss = PRIV1_DATA_SS;    
	} 
	else if (priv==1) 
	{
		thr_states[id].ldt_sel |= 0x2;
		thr_states[id].cs = PRIV2_CODE_SS;
		thr_states[id].ds = PRIV2_DATA_SS;
		thr_states[id].es = PRIV2_DATA_SS;
		thr_states[id].fs = PRIV2_HIMEM_SS;
		thr_states[id].ss = PRIV2_DATA_SS;       
	} 
	else 
	{
		thr_states[id].ldt_sel |= 0x3;
		thr_states[id].cs = PRIV3_CODE_SS;
		thr_states[id].ds = PRIV3_DATA_SS;
		thr_states[id].es = PRIV3_DATA_SS;
		thr_states[id].fs = 0;
		thr_states[id].ss = PRIV3_DATA_SS;    
	}
	thr_states[id].esi = 0;
	thr_states[id].edi = 0;
	thr_states[id].ebx = 0;
	
	if (priv < 2) 
		thr_states[id].eflags = 0x00002002;
	else 
		thr_states[id].eflags = 0x00002202; /* bits 12,13: iopl=2; 
					                           bit 9: int enable; bit 1: reserved */  
}


