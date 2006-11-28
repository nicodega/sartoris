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

struct tss      thr_tss[MAX_THR];
unsigned char   stacks[MAX_THR][STACK0_SIZE];

void build_tss(int id, int task_num, int priv, void *ep, void *stack) {
  
  thr_tss[id].io_map = 0;
  thr_tss[id].ldt_sel = ((GDT_LDT + task_num) << 3);
  thr_tss[id].gs = 0;
  thr_tss[id].esp = (unsigned int)stack;  
  thr_tss[id].ebp = (unsigned int)stack;
  
  thr_tss[id].ss0 = KRN_DATA_SS;
  thr_tss[id].esp0 = (unsigned int)(&stacks[id][0]) + STACK0_SIZE - 4;
  
  if (priv==0) {
    thr_tss[id].ldt_sel |= 0x1;
    thr_tss[id].cs = PRIV1_CODE_SS;
    thr_tss[id].ds = PRIV1_DATA_SS;
    thr_tss[id].es = PRIV1_DATA_SS;
    thr_tss[id].fs = PRIV1_HIMEM_SS;
    thr_tss[id].ss = PRIV1_DATA_SS;
    thr_tss[id].ss1 = PRIV1_DATA_SS;
    thr_tss[id].esp1 = (unsigned int)stack;
    thr_tss[id].esp2 = 0;
    thr_tss[id].ss2 = 0;
  } else if (priv==1) {
    thr_tss[id].ldt_sel |= 0x2;
    thr_tss[id].cs = PRIV2_CODE_SS;
    thr_tss[id].ds = PRIV2_DATA_SS;
    thr_tss[id].es = PRIV2_DATA_SS;
    thr_tss[id].fs = PRIV2_HIMEM_SS;
    thr_tss[id].ss = PRIV2_DATA_SS;
    thr_tss[id].ss1 = 0;
    thr_tss[id].esp1 = 0;
    thr_tss[id].ss2 = PRIV2_DATA_SS;
    thr_tss[id].esp2 = (unsigned int)stack;    
  } else {
    thr_tss[id].ldt_sel |= 0x3;
    thr_tss[id].cs = PRIV3_CODE_SS;
    thr_tss[id].ds = PRIV3_DATA_SS;
    thr_tss[id].es = PRIV3_DATA_SS;
    thr_tss[id].fs = 0;
    thr_tss[id].ss = PRIV3_DATA_SS;
    thr_tss[id].ss1 = 0;
    thr_tss[id].esp1 = 0;
    thr_tss[id].ss2 = 0;
    thr_tss[id].esp2 = 0;
  }
  thr_tss[id].esi = 0;
  thr_tss[id].edi = 0;
  thr_tss[id].eax = 0;
  thr_tss[id].ebx = 0;
  thr_tss[id].ecx = 0;
  thr_tss[id].edx = 0;
  if (priv < 2) {
    thr_tss[id].eflags = 0x00002002;
  } else {
    thr_tss[id].eflags = 0x00002202; /* bits 12,13: iopl=2; 
					bit 9: int enable; bit 1: reserved */
  }
  thr_tss[id].eip = (unsigned int)ep;
  thr_tss[id].cr3 = 0;
  
  thr_tss[id].back_link = 0;
}
