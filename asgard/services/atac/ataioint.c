/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*	NOTE: This code is based on ARADRVR by Hale Landis (hlandis@ata-atapi.com)
*	Hale Landis library source code is distributed under this conditions: 
*
*	"There is no copyright and there are no restrictions on the use
*	of this ATA Low Level I/O Driver code.  It is distributed to
*	help other programmers understand how the ATA device interface
*	works and it is distributed without any warranty.  Use this
*	code at your own risk."
*
*	I thought it would be worth mentioning I'm pretty grateful for 
*	his library, for it has helped a lot on ata interface undertanding.
*
*/

#include "atac.h"


static void int_handler( void );        // our INT handler

// system interrupt controller data...

#define PIC0_CTRL 0x20           // PIC0 i/o address
#define PIC0_MASK 0x21           // PIC0 i/o address
#define PIC1_CTRL 0xA0           // PIC1 i/o address
#define PIC1_MASK 0xA1           // PIC1 i/o address

#define PIC0_ENABLE_PIC1 0xFB    // mask to enable PIC1 in PIC0

static pic_enable_irq[8] =       // mask to enable
   { 0xFE, 0xFD, 0xFB, 0xF7,     //   IRQ 0-7 in PIC 0 or
     0xEF, 0xDF, 0xBF, 0x7F  };  //   IRQ 8-15 in PIC 1

#define PIC_EOI      0x20        // end of interrupt

//*************************************************************
//
// Enable interrupt mode for a channel
//
//*************************************************************

int int_enable_irq(struct ata_channel *channel)
//  irq: 1 to 15
//  bm_addr: i/o address for BMCR/BMIDE Status register
//  ata_addr: i/o address for the ATA Status register
{

	// error if interrupts enabled now
	// error if invalid irq number
	if ( channel->int_use_intr_flag )
		return 1;
	if ( ( channel->irq < 1 ) || ( channel->irq > 15 ) )
		return 2;
	if ( channel->irq == 2 )
		return 2;

	// create the int handler thread
	struct pm_msg_create_thread msg_create_thr;
	
	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = channel->id;
	msg_create_thr.response_port = ATAC_THREAD_ACK_PORT;
	msg_create_thr.task_id = get_current_task();
	msg_create_thr.flags = 0;
	msg_create_thr.interrupt = channel->irq + 32;
	msg_create_thr.entry_point = &channel_wp;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	unsigned short res = 0;
	// wait for THREAD_CREATED_EVENT driven signal
	wait_signal(THREAD_CREATED_EVENT, get_current_task(), SIGNAL_TIMEOUT_INFINITE, msg_create_thr.req_id, SIGNAL_PARAM_IGNORE, &res, NULL);

	if (!res) 
	{
		return 1; // FAIL!
	}

	// interrupts use is now enabled

	channel->int_use_intr_flag = 1;
	channel->int_intr_flag = 0;
	
	// put channel address on the stack (ajjjjjjjjjjj!)

	__asm__ __volatile__("movl %0, %%eax;movl %%eax, -256(%%esp) " : : "m" (channel) : "eax");

	return 0;
}



//*************************************************************
//
// Interrupt Handler.
//
//*************************************************************

static void int_handler( void )
{
	// ok... I really didn't know how to do this, hence I'll do it in an 
	// awful way. What I have to do is tell this interrupt handler
	// which channel it's in charge of, and I will do that
	// by leaving it's pointer on the stack space, upon creation.
	// Pointer will be left at esp - 256... it ugly I know that :D, but it works.
	struct ata_channel *channel = NULL;

	__asm__ __volatile__("movl -256(%%esp),%0 " : "=r" (channel) :);

	for(;;)
	{
		// increment the interrupt counter
		channel->int_intr_cntr ++ ;
		// check it's not like SIGNAL_IGNORE_PARAM
		if(channel->int_intr_cntr == SIGNAL_PARAM_IGNORE) channel->int_intr_cntr ++;

		// if BMCR/BMIDE present read the BMCR/BMIDE status
		// else just read the device status.

		if ( channel->int_bmcr_addr )
		{
			// PCI ATA controller...
			// ... read BMCR/BMIDE status
			channel->int_bm_status = inb( channel->int_bmcr_addr );
			//... check if Interrupt bit = 1
			if ( channel->int_bm_status & 0x04 )
			{
				// ... Interrupt=1...
				// ... read ATA status,
				// ... reset Interrupt bit.
				// ... send event
				channel->int_intr_flag++ ;
				channel->int_ata_status = inb( channel->int_ata_addr );
				outb( channel->int_bmcr_addr, 0x04 );

				send_event(get_current_task(), IRQ_EVENT, channel->id, 0, 0, 0);
			}
		}
		else
		{
			// legacy ATA controller...
			// ... read ATA status.
			// send event
			channel->int_intr_flag++;
			channel->int_ata_status = inb( channel->int_ata_addr );

			send_event(get_current_task(), IRQ_EVENT, channel->id, 0, 0, 0);
		}

		  
		// send End-of-Interrupt (EOI) to the interrupt controller(s).
		outb( PIC0_CTRL, PIC_EOI );
		if ( channel->irq >= 8 )
			outb( PIC1_CTRL, PIC_EOI );			

		ret_from_int();	// return from interrupt
	}
}
