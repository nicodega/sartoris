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

/* Structure for APIC */
typedef struct
{
	unsigned char id;		// will be the APIC ID
	unsigned char present;	// 1 if present
} APIC;

#define MAX_APICS 32
#define MAX_IOAPICS 8

/* Structure for IO APICs */
typedef struct
{
    unsigned char index;		// The IO APIC‘s index
	unsigned char id;			// The IO APIC‘s ID.
	unsigned char present;		// 1 if present
    unsigned char ints;		    // ioapic ints
	void *address;              // The physical address to access this IO APIC. Each IO APIC resides at a unique address
	unsigned int systemVectorBase;
} IOAPIC;

extern unsigned int idt_call_table[];
extern unsigned int exceptions_call_table[];
extern unsigned int int_code_start;

void reprog_pics(void);
void eoi_int_master();
void eoi_int_slave();
void enable_int_master(int irq);
void disable_int_master(int irq);
void enable_int_slave(int irq);
void disable_int_slave(int irq);
int arch_switch_thread_int(struct thr_state *thr, unsigned int cr3, void *eip, void *stack);

void init_ioapic(unsigned char apicid, void *base, unsigned int global_vector_start);
void start_apic(void *base);
unsigned char ioapic_max_ints(void *base);
void ioapic_map_int(void *address, int intnum, unsigned char vector, int polarity, int tgmode, int intstate, unsigned char apicid);
void ioapic_set_int_state(void *address, int intnum, int state);
void apic_eoi();
void arch_interrupts_paging();

#define IOAPIC_INTSTATE_DISABLE 0
#define IOAPIC_INTSTATE_ENABLE  1

#define IOAPIC_POLARITY_HIGHACTIVE 0
#define IOAPIC_POLARITY_LOWACTIVE  1

#define IOAPIC_TRIGMODE_EDGE    0
#define IOAPIC_TRIGMODE_LEVEL   1

#endif
