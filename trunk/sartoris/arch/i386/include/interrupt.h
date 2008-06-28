/*  
 *   Sartoris 0.5 i386 interrupt handling support header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#include "tss.h"

#ifndef __INTERRUPTS_H
#define __INTERRUPTS_H

extern unsigned int idt_call_table[];
extern unsigned int exceptions_call_table[];
extern unsigned int int_code_start;

int ret_from_int(void);

void static hook_irq(int irq, int thread_id, int dpl);
void reprog_pics(void);
void eoi_master(void);
void eoi_slave(void);
void enable_int_master(int irq);
void disable_int_master(int irq);
void enable_int_slave(int irq);
void disable_int_slave(int irq);
int arch_switch_thread_int(struct thr_state *thr, unsigned int cr3, void *eip, void *stack);
#endif
