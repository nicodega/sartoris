/*  
 *   Sartoris microkernel i386 interrupt-handling functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "interrupt.h"
#include "descriptor.h"

struct seg_desc idt[MAX_IRQ];

int exc_error_code;

int arch_create_int_handler(int number) {
  unsigned int ep;
  
  if (number >= MAX_IRQ) { return -1; }

  ep = idt_call_table[number];
  idt[number].dword0 = (KRN_CODE << 16) | (ep & 0xffff);
  idt[number].dword1 = (ep & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | 0x800;
  
  if ( (number > 31) && (number < 48) ) {
    /* enable the line in the pic */
    number -= 32;
    if (0 <= number && number < 8) {
      enable_int_master(number);
    } else {
      if (8 <= number && number < 16 && number != 9) { 
	enable_int_slave(number-8);
      }
    }
  }
  return 0;
}

int arch_destroy_int_handler(int number) {
  
  if (number >= MAX_IRQ) { return -1; }
  
  if ( (number > 31) && (number < 48) ) {
    /* disable the line in the pic */
    number -= 32;
    if (0 <= number && number < 8) {
      disable_int_master(number);
    } else {
      if (8 <= number && number < 16 && number != 9) { 
	disable_int_slave(number-8);
      }
    }
  }
  
  return 0;
}

void init_interrupts() 
{
	int i;
	unsigned int l_desc[2];

	/* set up default exception handlers */

	for (i=0; i<32; i++) 
	{
		/* REMOVE THIS, IS IN ORDER TO FORCE BOCHS TO STOP ON GPF FOR DEBUGGING */
		idt[i].dword0 = G_DW0_OFFSET((unsigned int) exceptions_call_table[i]) | G_DW0_SELECTOR(0x8);
		idt[i].dword1 = G_DW1_OFFSET((unsigned int) exceptions_call_table[i]) | IRQ_GATE_32;		
	}

	/* zero out the rest */

	for (i=32; i<MAX_IRQ; i++) {
		idt[i].dword0 = 0;
		idt[i].dword1 = 0;
	}

   /* new idt load */
	l_desc[0] = MAX_IRQ * sizeof(struct seg_desc) + ((((unsigned int) idt) & 0xffff) << 16);
	l_desc[1] = (unsigned int) idt >> 16;
	__asm__ ("lidt %0" : : "m"(*l_desc) );

}

static void hook_irq(int irq, int thread_id, int dpl) {
  unsigned int ep;
  
  ep = idt_call_table[irq];
  idt[irq].dword0 = (KRN_CODE << 16) | (ep & 0xffff);
  idt[irq].dword1 = (ep & 0xffff0000) | DESC_DPL(dpl) | IRQ_GATE_32 | 0x800;
}
