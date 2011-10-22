/*  
 *   Sartoris microkernel i386 interrupt-handling functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@gmail.com      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "interrupt.h"
#include "descriptor.h"
#include "caps.h"

struct seg_desc idt[MAX_IRQ] __align(8); // (intel says its better if its 8 byte aligned)

unsigned int exc_error_code;
unsigned int exc_int_addr;
int int7handler;                // if it's 1 then there's a handler for int7
int int1handler;                // if it's 1 then there's a handler for int1
extern void default_int();

int arch_create_int_handler(int number) 
{
	unsigned int ep;

	if (number >= MAX_IRQ) { return -1; }

#ifdef FPU_MMX
	/* This will be handled by us. */
	if(number == 7 && arch_has_cap_or(SCAP_MMX | SCAP_FPU | SCAP_SSE | SCAP_SSE2))
	{
        int7handler = 1;
		return 0;
	}
#endif
    if(number == 1)
    {
        int1handler = 1;
        return 0;
    }
    
	ep = idt_call_table[number];
    
	idt[number].dword0 = (KRN_CODE << 16) | (ep & 0xffff);
	idt[number].dword1 = (ep & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | GATE_32;

	if ( (number > 31) && (number < 48) ) 
	{        
		/* enable the line in the pic */
		number -= 32;
		if (0 <= number && number < 8) 
		{
			enable_int_master(number);
		} 
		else 
		{
			if (8 <= number && number < 16 && number != 9) 
			{ 
				enable_int_slave(number-8);
			}
		}
	}    
	return 0;
}

int arch_destroy_int_handler(int number) 
{
	if (number >= MAX_IRQ) { return -1; }

    if(number == 7 && arch_has_cap_or(SCAP_MMX | SCAP_FPU | SCAP_SSE | SCAP_SSE2))
	{
        int7handler = 0;
		return 0;
	}
    if(number == 1)
    {
        int1handler = 0;
        return 0;
    }

	if ( (number > 31) && (number < 48) ) 
	{
		/* disable the line in the pic */
		number -= 32;
		if (0 <= number && number < 8) 
		{
			disable_int_master(number);
		} 
		else 
		{
			if (8 <= number && number < 16 && number != 9) 
			{ 
				disable_int_slave(number-8);
			}
		}
	}

	/* if it was an exception interrupt, 
	attach default handler */
	if(number < 32)
	{
		idt[number].dword0 = G_DW0_OFFSET((unsigned int) exceptions_call_table[number]) | G_DW0_SELECTOR(0x8);
		idt[number].dword1 = G_DW1_OFFSET((unsigned int) exceptions_call_table[number]) | IRQ_GATE_32;
	}
	else if(number == 0x27)
	{
		/* Trap irq 7 now that it's no longer handled, in case a spurious int is rised. */
		idt[0x27].dword0 = (KRN_CODE << 16) | (((unsigned int) default_int) & 0xffff);
		idt[0x27].dword1 = (((unsigned int) default_int) & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | 0x800;
	}
  
	return 0;
}

void init_interrupts() 
{
	int i;
	unsigned int l_desc[2];

    int7handler = NULL;

	/* set up default exception handlers */
	for (i=0; i<32; i++) 
	{
		idt[i].dword0 = G_DW0_OFFSET((unsigned int) exceptions_call_table[i]) | G_DW0_SELECTOR(KRN_CODE);
		idt[i].dword1 = G_DW1_OFFSET((unsigned int) exceptions_call_table[i]) | IRQ_GATE_32 | GATE_32;		
	}

	/* zero out the rest */
	for (i=32; i<MAX_IRQ; i++) 
	{
		idt[i].dword0 = 0;
		idt[i].dword1 = 0;
	}

	// On new bochs I'm getting an IRQ 7... can it be an APIC spurious interrupt? */
	idt[0x27].dword0 = (KRN_CODE << 16) | (((unsigned int) default_int) & 0xffff);
	idt[0x27].dword1 = (((unsigned int) default_int) & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | GATE_32;

   /* new idt load */
	l_desc[0] = MAX_IRQ * sizeof(struct seg_desc) + ((((unsigned int) idt) & 0xffff) << 16);
	l_desc[1] = (unsigned int) idt >> 16;
	__asm__ ("lidt %0" : : "m"(*l_desc) );

}
