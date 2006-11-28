#ifndef __TSS_H
#define __TSS_H

#define STACK0_SIZE 0x400     /* 1 kb */

/* the back_link and all the segment selectors 
 * (ss0, ss1, ss2, es, cs, ss, ds, fs, gs, ldt_sel)
 * must have their higher 16 bits set to zero
 */

struct tss {
  unsigned int back_link;
  unsigned int esp0;
  unsigned int ss0;
  unsigned int esp1;
  unsigned int ss1;
  unsigned int esp2;
  unsigned int ss2;
  unsigned int cr3;
  unsigned int eip;
  unsigned int eflags;
  unsigned int eax;
  unsigned int ecx;
  unsigned int edx;
  unsigned int ebx;
  unsigned int esp;
  unsigned int ebp;
  unsigned int esi;
  unsigned int edi;
  unsigned int es;
  unsigned int cs;
  unsigned int ss;
  unsigned int ds;
  unsigned int fs;
  unsigned int gs;
  unsigned int ldt_sel;
  unsigned int io_map;
};

void build_tss(int id, int task_num, int priv, void *ep, void *stack);

#endif


