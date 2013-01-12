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
#include "acpi.h"
#include "paging.h"

struct seg_desc idt[MAX_IRQ] __align(8); // (intel says its better if its 8 byte aligned)

unsigned int exc_error_code;
unsigned int exc_int_addr;
int int7handler;                // if it's 1 then there's a handler for int7
int int1handler;                // if it's 1 then there's a handler for int1
extern void default_int();

APIC apics[MAX_APICS];
IOAPIC ioapics[MAX_IOAPICS];
int apicsLen = 0;
int ioapicsLen = 0;
void *localApicAddress;
unsigned short vector_ioapic[MAX_IRQ];   // if vector is mapped to an IOAPIC it's ID will be here on the first byte and it's int num on the second
unsigned char hard_int_vector[MAX_IRQ];

#define HARD_INT_NO_VECTOR 0x00

#define VECTOR_IOAPIC(v) (vector_ioapic[v] & 0x00FF)
#define VECTOR_IOAPIC_INT(v) ((vector_ioapic[v] & 0xFF00) >> 8)
#define MAKE_VECTOR_IOAPIC(index,intnum) (unsigned short)( (((unsigned short)index) & 0x00FF) | (((unsigned short)(intnum << 8)) & 0xFF00) )
#define VECTOR_NO_IOAPIC 0x00FF

extern unsigned int hard_ints_count;
extern int apic_eoi_broadcast;               // if 1 eoi broadcast for apics is enabled

IOAPIC *hard_int_apic(int gint);

unsigned int apic_mapped[0x400]  __align(PG_SIZE); // we will map the apic here

/* 
This function will map a hardware interrupt to a vector with the specified params.
*/
int ARCH_FUNC_ATT3 arch_map_hard_int(int hard_int, int vector, int *arch_params)
{
    if(hard_int >= hard_ints_count || vector < 32 || ioapicsLen == 0)
        return -1; // FAIL

    if(VECTOR_IOAPIC(vector) != VECTOR_NO_IOAPIC) // already mapped to a hardware int
        return -1;

    int polarity = *arch_params;
    int trigger_mode = *(arch_params+1);

    IOAPIC *ioapc = hard_int_apic(hard_int);

    // free the old vector of the hard int
    if(hard_int_vector[hard_int] != HARD_INT_NO_VECTOR)
        vector_ioapic[hard_int_vector[hard_int]] = VECTOR_NO_IOAPIC;

    vector_ioapic[vector] =  MAKE_VECTOR_IOAPIC(ioapc->index, (hard_int - ioapc->systemVectorBase));

    map_page(ioapc->address);

    // map the int disabled
    ioapic_map_int((void*)AUX_PAGE_SLOT(curr_thread), (hard_int - ioapc->systemVectorBase), vector, polarity, trigger_mode, IOAPIC_INTSTATE_DISABLE, apics[0].id);

    return 0;
}

int ARCH_FUNC_ATT1 arch_create_int_handler(int vector) 
{
	unsigned int ep;

	if (vector > MAX_IRQ) { return -1; }

#ifdef FPU_MMX
	/* This will be handled by us. */
	if(vector == 7 && arch_has_cap_or(SCAP_MMX | SCAP_FPU | SCAP_SSE | SCAP_SSE2))
	{
        int7handler = 1;
		return 0;
	}
#endif
    if(vector == 1)
    {
        int1handler = 1;
        return 0;
    }
    
	ep = idt_call_table[vector];
    
	idt[vector].dword0 = (KRN_CODE << 16) | (ep & 0xffff);
	idt[vector].dword1 = (ep & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | GATE_32;

	if(apicsLen == 0)
	{
		if ( (vector > 31) && (vector < 48) ) 
		{        
			/* enable the line in the pic */
			vector -= 32;
			if (0 <= vector && vector < 8) 
			{
				enable_int_master(vector);
			} 
			else 
			{
				if (8 <= vector && vector < 16 && vector != 9) 
				{ 
					enable_int_slave(vector-8);
				}
			}
		}
	}
	else
	{

        // we must findout if the int is apped to an IOAPIC
        if(VECTOR_IOAPIC(vector) != VECTOR_NO_IOAPIC)
        {
            IOAPIC *ioapc = &ioapics[VECTOR_IOAPIC(vector)];

            // map the IOAPIC on our thread slot!
            map_page(ioapc->address);
            
            bprintf("Unmasking int on IOAPIC vec: %i ioapic: %i\n", vector, VECTOR_IOAPIC(vector));
            // enable the int int the IOAPIC
            ioapic_set_int_state(AUX_PAGE_SLOT(curr_thread), VECTOR_IOAPIC_INT(vector), IOAPIC_INTSTATE_ENABLE);
        }
	}

	return 0;
}

int ARCH_FUNC_ATT1 arch_destroy_int_handler(int vector) 
{
	if (vector >= MAX_IRQ) { return -1; }

    if(vector == 7 && arch_has_cap_or(SCAP_MMX | SCAP_FPU | SCAP_SSE | SCAP_SSE2))
	{
        int7handler = 0;
		return 0;
	}
    if(vector == 1)
    {
        int1handler = 0;
        return 0;
    }

	if(apicsLen == 0)
	{
		if ( (vector > 31) && (vector < 48) ) 
		{
			/* disable the line in the pic */
			vector -= 32;
			if (0 <= vector && vector < 8) 
			{
				disable_int_master(vector);
			} 
			else 
			{
				if (8 <= vector && vector < 16 && vector != 9) 
				{ 
					disable_int_slave(vector-8);
				}
			}
		}
	}
	else
	{        
		// we must findout if the int is apped to an IOAPIC
        if(VECTOR_IOAPIC(vector) != VECTOR_NO_IOAPIC)
        {
            IOAPIC *ioapc = &ioapics[VECTOR_IOAPIC(vector)];

            map_page(ioapc->address);
   
            // enable the int int the IOAPIC
            ioapic_set_int_state(AUX_PAGE_SLOT(curr_thread), VECTOR_IOAPIC_INT(vector), IOAPIC_INTSTATE_DISABLE);
        }
	}

	/* if it was an exception interrupt, 
	attach default handler */
	if(vector < 32)
	{
		idt[vector].dword0 = G_DW0_OFFSET((unsigned int) exceptions_call_table[vector]) | G_DW0_SELECTOR(0x8);
		idt[vector].dword1 = G_DW1_OFFSET((unsigned int) exceptions_call_table[vector]) | IRQ_GATE_32;
	}
	else if(vector == 0x27)
	{
		/* Trap irq 7 now that it's no longer handled, in case a spurious int is rised. */
		idt[0x27].dword0 = (KRN_CODE << 16) | (((unsigned int) default_int) & 0xffff);
		idt[0x27].dword1 = (((unsigned int) default_int) & 0xffff0000) | DESC_DPL(0) | IRQ_GATE_32 | 0x800;
	}
  
	return 0;
}

/* Invoke after paging is enabled */
void arch_interrupts_paging()
{
    // map the apic to the kernel
    kernel_map_phy(apic_mapped, localApicAddress);

    localApicAddress = apic_mapped;
}

void arch_init_interrupts() 
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

	/* Find out if this system is using APIC or the old PIC */
	
	for(i = 0; i < MAX_APICS;i++)
		apics[i].present = 0;

	for(i = 0; i < MAX_IOAPICS;i++)
		ioapics[i].present = 0;
	
	/* Paging is not yet initialized, yay! */
	RSDPDescriptor *rsdp = find_RSDP();

	reprog_pics();	// this will initialize the PIC with all ints masked

	if(rsdp == NULL) // whe should use intel's MP spec here, but let's leave it for other time :) and just assume standard PIC
	{
		bprintf("APIC: RSDP Not Found\n");
		return;
	}
    
	FADT *fadt = findOnRSDP(rsdp, "FACP", 4);

	if(fadt == NULL)
	{
		bprintf("APIC: FADT Not Found\n");
		return;
	}

	if(fadt->IntModel != 0)
	{
		MADT *madt = findOnRSDP(rsdp, "APIC", 4);

		if(madt == NULL)
		{
			bprintf("APIC: MADT Not Found\n");
			return;
		}
	
		localApicAddress = (void*)madt->localApicAddress;
			
        bprintf("APIC: APIC Address %x\n", localApicAddress);

		// go through MADT
		unsigned int rem = madt->h.Length - sizeof(MADT); 
		unsigned char *ptrType = (unsigned char*)((unsigned int)madt + sizeof(MADT));
		MADT_INTOVR *int_overrides[16];
        int ov = 0;

		for(i = 0; i < 16; i++)
			int_overrides[16] = NULL;

		while(rem > 0)
		{
			if(*ptrType == MADT_TYPE_LAPIC)
			{
				MADT_APIC *lapic = (MADT_APIC*)ptrType;
						
				apics[apicsLen].id = lapic->APIC_ID;
				apics[apicsLen].present = 1;
			
				bprintf("APIC: Id %i Detected\n", ((unsigned int)lapic->APIC_ID) & 0x000000FF);

				apicsLen++;

				rem -= lapic->length;
                ptrType += lapic->length;
			}
			else if(*ptrType == MADT_TYPE_IOAPIC)
			{
				MADT_IOAPIC *ioapic = (MADT_IOAPIC*)ptrType;

				ioapics[ioapicsLen].id = ioapic->IOAPICID;
				ioapics[ioapicsLen].address = (void*)ioapic->address;
				ioapics[ioapicsLen].systemVectorBase = ioapic->systemVectorBase;
				ioapics[ioapicsLen].present = 1;
                ioapics[ioapicsLen].index = ioapicsLen;
                ioapics[ioapicsLen].ints = ioapic_max_ints((void*)ioapic->address);

				bprintf("IOAPIC: Id %i, addr: %x, vb: %x ints: %i Detected\n", (((unsigned int)ioapic->IOAPICID) & 0x000000FF), ioapics[ioapicsLen].address, ioapic->systemVectorBase, ioapics[ioapicsLen].ints);

				ioapicsLen++;

				rem -= ioapic->length;
                ptrType += ioapic->length;
			}
			else if(*ptrType == MADT_TYPE_INTOVERRIDE)
			{
				MADT_INTOVR *intov = (MADT_INTOVR*)ptrType;

                int_overrides[ov++] = intov;

				bprintf("INTOVERRIDE: Int %i glb: %i Detected\n", intov->source, intov->globalSystemInterruptVector);
                
				rem -= intov->length;
                ptrType += intov->length;
			}
		}
		
		if(apicsLen > 0)
		{            
            for(i = 0; i < MAX_IRQ; i++)
            {
                vector_ioapic[i] = VECTOR_NO_IOAPIC;
                hard_int_vector[i] = HARD_INT_NO_VECTOR;
            }

			// init IOAPICS with all ints disabled
			for(i = 0; i < ioapicsLen; i++)
			{
                bprintf("APIC: INIT IOAPIC %i of %i address %i ints %i\n", ioapics[i].id, ioapicsLen, ioapics[i].address, ioapics[i].ints);
                // init ioapic will map ints to 32 + ioapic.systemVectorstart + int vector
				init_ioapic(apics[i].id, ioapics[i].address, ioapics[i].systemVectorBase);
			}
            
            bprintf("APIC: IOAPICS INITIALIZED\n");

            // map ISA ints (ensure prolarity etc)
            IOAPIC *ioapc = hard_int_apic(0);

             for(i = 0; i < hard_ints_count; i++)
                hard_int_vector[i] = 32+i;

            for(i = 0; i < 16; i++)
            {
                vector_ioapic[32+i] = MAKE_VECTOR_IOAPIC(ioapc->index,i);
                bprintf("APIC: MAP ISA iint: %i vec: %i pol: %i trg: %i state: %i\n", i, 32+i, IOAPIC_POLARITY_LOWACTIVE, IOAPIC_TRIGMODE_EDGE, IOAPIC_INTSTATE_DISABLE);
                ioapic_map_int(ioapc->address, i, 32+i, IOAPIC_POLARITY_LOWACTIVE, IOAPIC_TRIGMODE_EDGE, IOAPIC_INTSTATE_DISABLE, apics[0].id);                
            }
                        
            bprintf("APIC: ISA INTS MAPPED (global 0-16 to 32-48)\n");

			// map ISA INTs to its PIC equivalents using overrides
            if(ov > 0)
            {
                for(i = 0; i < ov; i++)
                {
                    MADT_INTOVR *intov = int_overrides[i];

                    ioapc = hard_int_apic(intov->globalSystemInterruptVector);

                    unsigned int pol = MADT_INTOVR_FLAG_POLARITY(intov->flags);
                    if((pol & MADT_INTOVR_FLAG_POLARITY_BUS) != 0 || (pol & MADT_INTOVR_FLAG_POLARITY_LOW) != 0)
                        pol = IOAPIC_POLARITY_LOWACTIVE;
                    else
                        pol = IOAPIC_POLARITY_HIGHACTIVE;

                    unsigned int tg = MADT_INTOVR_FLAG_TRIGMODE(intov->flags);
                    if((tg & MADT_INTOVR_FLAG_TRIGMODE_BUS) != 0 || (tg & MADT_INTOVR_FLAG_TRIGMODE_EDGE) != 0)
                        tg = IOAPIC_TRIGMODE_EDGE;
                    else
                        tg = IOAPIC_TRIGMODE_LEVEL;
                    
                    int intnum = (intov->globalSystemInterruptVector - ioapc->systemVectorBase);

                    hard_int_vector[intov->source] = HARD_INT_NO_VECTOR;
                    hard_int_vector[intnum] = intov->source + 32;
                    vector_ioapic[intov->source + 32] = MAKE_VECTOR_IOAPIC(ioapc->index,intnum);
                    
                    // map the int disabled
                    ioapic_map_int(ioapc->address, intnum, intov->source + 32, pol, tg, IOAPIC_INTSTATE_DISABLE, apics[0].id);

                    bprintf("APIC: REMAP ISA iint: %i vec: %i pol: %i trg: %i state: %i\n", i, intov->source + 32, pol, tg, IOAPIC_INTSTATE_DISABLE);
                    bprintf("APIC: Global int %i is now mapped to vector %i\n", intnum, intov->source + 32);
                }
            }
           
			// initialize this processor APIC
			start_apic(localApicAddress);

            bprintf("APIC: Started\n");
		}
	}
	else 
	{
		bprintf("APIC: Int model 0\n");
		return;
	}
}

IOAPIC *hard_int_apic(int gint)
{
    int i;
    for(i = 0;i < ioapicsLen; i++)
    {
        if(ioapics[i].systemVectorBase <= gint && gint < ioapics[i].systemVectorBase + ioapics[i].ints)
            return &ioapics[i];
    }
    return NULL;
}

void ARCH_FUNC_ATT1 arch_eoi(int vector)
{
    if(apicsLen > 0)
	{
        if(vector_ioapic[vector] != VECTOR_NO_IOAPIC)
            apic_eoi();        
    }
    else
    {
        vector -= 32;
		if (vector < 8) 
		{
			eoi_int_master(vector);
		} 
		else if (8 <= vector && vector < 16 && vector != 9) 
		{ 
			eoi_int_slave(vector-8);
		}
    }
}
