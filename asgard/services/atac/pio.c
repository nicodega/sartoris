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

//********************************************************************
// This C source contains the low level I/O port IN/OUT functions.
//********************************************************************

#include "atac.h"
#include "ata.h"
#include <lib/io.h>

void delay_400(struct ata_channel *channel)
{
	int j = 0;
	for (; j < 14; j++) pio_inbyte( channel, CB_ASTAT );
}

//*************************************************************
//
// Set the host adapter i/o base addresses.
//
//*************************************************************

void pio_set_iobase_addr(struct ata_channel *channel, unsigned int base1, unsigned int base2 )
{
	channel->pio_base_addr1 = base1;
	channel->pio_base_addr2 = base2;
	channel->pio_linear = 0;
	channel->pio_reg_addrs[ CB_DATA ] = channel->pio_base_addr1 + 0;  // 0
	channel->pio_reg_addrs[ CB_FR   ] = channel->pio_base_addr1 + 1;  // 1
	channel->pio_reg_addrs[ CB_SC   ] = channel->pio_base_addr1 + 2;  // 2
	channel->pio_reg_addrs[ CB_SN   ] = channel->pio_base_addr1 + 3;  // 3
	channel->pio_reg_addrs[ CB_CL   ] = channel->pio_base_addr1 + 4;  // 4
	channel->pio_reg_addrs[ CB_CH   ] = channel->pio_base_addr1 + 5;  // 5
	channel->pio_reg_addrs[ CB_DH   ] = channel->pio_base_addr1 + 6;  // 6
	channel->pio_reg_addrs[ CB_CMD  ] = channel->pio_base_addr1 + 7;  // 7
	channel->pio_reg_addrs[ CB_DC   ] = channel->pio_base_addr2 + 6;  // 8
	channel->pio_reg_addrs[ CB_DA   ] = channel->pio_base_addr2 + 7;  // 9
}

//*************************************************************
//
// Set the host adapter memory base addresses.
//
//*************************************************************

void pio_set_memory_addr(struct ata_channel *channel, unsigned int physical )
{
	channel->pio_base_addr1 = 0;
	channel->pio_base_addr2 = 8;
	channel->pio_linear = physical;
	channel->pio_memory_dt_opt = PIO_MEMORY_DT_OPT0;
	channel->pio_reg_addrs[ CB_DATA ] = channel->pio_base_addr1 + 0;  // 0
	channel->pio_reg_addrs[ CB_FR   ] = channel->pio_base_addr1 + 1;  // 1
	channel->pio_reg_addrs[ CB_SC   ] = channel->pio_base_addr1 + 2;  // 2
	channel->pio_reg_addrs[ CB_SN   ] = channel->pio_base_addr1 + 3;  // 3
	channel->pio_reg_addrs[ CB_CL   ] = channel->pio_base_addr1 + 4;  // 4
	channel->pio_reg_addrs[ CB_CH   ] = channel->pio_base_addr1 + 5;  // 5
	channel->pio_reg_addrs[ CB_DH   ] = channel->pio_base_addr1 + 6;  // 6
	channel->pio_reg_addrs[ CB_CMD  ] = channel->pio_base_addr1 + 7;  // 7
	channel->pio_reg_addrs[ CB_DC   ] = channel->pio_base_addr2 + 6;  // 8
	channel->pio_reg_addrs[ CB_DA   ] = channel->pio_base_addr2 + 7;  // 9
}

//*************************************************************
//
// These functions do basic IN/OUT of byte and word values:
//
//    pio_inbyte()
//    pio_outbyte()
//    pio_inword()
//    pio_outword()
//
//*************************************************************

unsigned char pio_inbyte(struct ata_channel *channel, unsigned int addr )
{
	unsigned int reg_addr;
	unsigned char uc;
	
	reg_addr = channel->pio_reg_addrs[ addr ];
	if ( channel->pio_linear )
	{
		uc = *((unsigned char*)reg_addr);
	}
	else
	{
		uc = (unsigned char) inb( reg_addr );
	}
	channel->pio_last_read[ addr ] = uc;
	
	return uc;
}

//*************************************************************

void pio_outbyte(struct ata_channel *channel, unsigned int addr, unsigned char data )
{
	unsigned int reg_addr;

	reg_addr = channel->pio_reg_addrs[ addr ];
	if ( channel->pio_linear )
	{
		*((unsigned char*)reg_addr) = data;
	}
	else
	{
		outb( reg_addr, data );
	}
	channel->pio_last_write[ addr ] = data;
}

//*************************************************************

unsigned int pio_inword(struct ata_channel *channel, unsigned int addr )

{
	unsigned int reg_addr;
	unsigned int ui;

	reg_addr = channel->pio_reg_addrs[ addr ];
	if ( channel->pio_linear )
	{
		ui = *((unsigned int*)reg_addr);
	}
	else
	{
		ui = (unsigned int) inw( reg_addr );
	}
	return ui;
}

//*************************************************************

void pio_outword(struct ata_channel *channel, unsigned int addr, unsigned int data )
{
	unsigned int reg_addr;
	
	reg_addr = channel->pio_reg_addrs[ addr ];
	if ( channel->pio_linear )
	{
		*((unsigned int*)reg_addr) = data;
	}
	else
	{
		outw( reg_addr, data );
	}
}

//*************************************************************
//
// These functions are normally used to transfer DRQ blocks:
//
// pio_drq_block_in()
// pio_drq_block_out()
//
//*************************************************************

// Note: pio_drq_block_in() is the primary way perform PIO
// Data In transfers. It will handle 8-bit, 16-bit and 32-bit
// I/O based data transfers and 8-bit and 16-bit PCMCIA Memory
// mode transfers.

void pio_drq_block_in(struct ata_channel *channel, unsigned int addr_datareg, unsigned int buf_addr, long word_cnt )
{
	long bcnt;
	int mem_dt_opt;
	unsigned int data_regaddr;
	unsigned int *uip1;
	unsigned int *uip2;
	unsigned char *ucp1;
	unsigned char *ucp2;
	
    unsigned int reg_addr = channel->pio_reg_addrs[ addr_datareg ];

	// NOTE: word_cnt is the size of a DRQ data block/packet
	// in words. The maximum value of wordCnt is normally:
	// a) For ATA, 16384 words or 32768 bytes (64 sectors,
	//    only with READ/WRITE MULTIPLE commands),
	// b) For ATAPI, 32768 words or 65536 bytes
	//    (actually 65535 bytes plus a pad byte).

	if ( channel->pio_linear )
	{

		// PCMCIA Memory mode data transfer.

		// set Data reg address per pio_memory_dt_opt
		data_regaddr = 0x0;
		mem_dt_opt = channel->pio_memory_dt_opt;
				
		if ( mem_dt_opt == PIO_MEMORY_DT_OPT8 )
			data_regaddr = 0x8;

		if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
		{
			data_regaddr = 0x400;
		}

		if (  channel->pio_xfer_width == 8 )
		{
			// PCMCIA Memory mode 8-bit
			bcnt = word_cnt * 2;
			ucp1 = (unsigned char*)channel->pio_linear + data_regaddr;
			for ( ; bcnt > 0; bcnt -- )
			{
				ucp2 = (unsigned char*) buf_addr;
				* ucp2 = * ucp1;
				buf_addr++;
				if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
				{
					data_regaddr++;
					data_regaddr = ( data_regaddr & 0x03ff ) | 0x0400;
					ucp1 = (unsigned char*) channel->pio_linear + data_regaddr;
				}
			}
		}
		else
		{
			// PCMCIA Memory mode 16-bit
			uip1 = (unsigned int*) channel->pio_linear + data_regaddr;
			for ( ; word_cnt > 0; word_cnt -- )
			{
				uip2 = (unsigned int*)buf_addr;
				*uip2 = *uip1;
				buf_addr += 2;
				if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
				{
					data_regaddr += 2;
					data_regaddr = ( data_regaddr & 0x03fe ) | 0x0400;
					uip1 = (unsigned int*) channel->pio_linear + data_regaddr;
				}
			}
		}
	}
	else
	{
		int pxw;
		long wc;

		// adjust pio_xfer_width - don't use DWORD if wordCnt is odd.

		pxw = channel->pio_xfer_width;
		if ( ( pxw == 32 ) && ( word_cnt & 0x1 ) )
			pxw = 16;

		// Data transfer using INS instruction.
		// Break the transfer into chunks of 32768 or fewer bytes.

		while ( word_cnt > 0 )
		{
			if ( word_cnt > 16384 )
				wc = 16384;
			else
				wc = word_cnt;

			if ( pxw == 8 )
			{
				// do REP INSB
				rinb( reg_addr, buf_addr, wc * 2 );
			}
			else if ( pxw == 32 )
			{
				// do REP INSD
				rind( reg_addr, buf_addr, wc / 2 );
			}
			else
			{
				// do REP INSW
				rinw( reg_addr, buf_addr, wc );
			}
			buf_addr = buf_addr + ( wc * 2 );
			word_cnt = word_cnt - wc;
		}
	}

   return;
}

//*************************************************************

// Note: pio_drq_block_out() is the primary way perform PIO
// Data Out transfers. It will handle 8-bit, 16-bit and 32-bit
// I/O based data transfers and 8-bit and 16-bit PCMCIA Memory
// mode transfers.

void pio_drq_block_out(struct ata_channel *channel, unsigned int addr_datareg, unsigned int buf_addr, long word_cnt )

{
   	long bcnt;
	int mem_dt_opt;
	unsigned int data_regaddr;
	unsigned int *uip1;
	unsigned int *uip2;
	unsigned char *ucp1;
	unsigned char *ucp2;
   
	unsigned int reg_addr = channel->pio_reg_addrs[ addr_datareg ];

	// NOTE: word_cnt is the size of a DRQ data block/packet
	// in words. The maximum value of wordCnt is normally:
	// a) For ATA, 16384 words or 32768 bytes (64 sectors,
	//    only with READ/WRITE MULTIPLE commands),
	// b) For ATAPI, 32768 words or 65536 bytes
	//    (actually 65535 bytes plus a pad byte).

	if ( channel->pio_linear )
	{

		// PCMCIA Memory mode data transfer.

		// set Data reg address per pio_memory_dt_opt
		data_regaddr = 0x0;
		mem_dt_opt = channel->pio_memory_dt_opt;
				
		if ( mem_dt_opt == PIO_MEMORY_DT_OPT8 )
			data_regaddr = 0x8;

		if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
		{
			data_regaddr = 0x400;
		}

		if (  channel->pio_xfer_width == 8 )
		{
			// PCMCIA Memory mode 8-bit
			bcnt = word_cnt * 2;
			ucp2 = (unsigned char*)channel->pio_linear + data_regaddr;
			for ( ; bcnt > 0; bcnt -- )
			{
				ucp1 = (unsigned char*) buf_addr;
				* ucp2 = * ucp1;
				buf_addr++;
				if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
				{
					data_regaddr++;
					data_regaddr = ( data_regaddr & 0x03ff ) | 0x0400;
					ucp2 = (unsigned char*) channel->pio_linear + data_regaddr;
				}
			}
		}
		else
		{
			// PCMCIA Memory mode 16-bit
			uip2 = (unsigned int*) channel->pio_linear + data_regaddr;
			for ( ; word_cnt > 0; word_cnt -- )
			{
				uip1 = (unsigned int*)buf_addr;
				*uip2 = *uip1;
				buf_addr += 2;
				if ( mem_dt_opt == PIO_MEMORY_DT_OPTB )
				{
					data_regaddr += 2;
					data_regaddr = ( data_regaddr & 0x03fe ) | 0x0400;
					uip2 = (unsigned int*) channel->pio_linear + data_regaddr;
				}
			}
		}
	}
	else
	{
		int pxw;
		long wc;

		// adjust pio_xfer_width - don't use DWORD if wordCnt is odd.

		pxw = channel->pio_xfer_width;
		if ( ( pxw == 32 ) && ( word_cnt & 0x1 ) )
			pxw = 16;

		// Data transfer using INS instruction.
		// Break the transfer into chunks of 32768 or fewer bytes.

		while ( word_cnt > 0 )
		{
			if ( word_cnt > 16384L )
				wc = 16384;
			else
				wc = word_cnt;

			if ( pxw == 8 )
			{
				// do REP INSB
				routb( reg_addr, buf_addr, wc * 2 );
			}
			else
			if ( pxw == 32 )
			{
				// do REP INSD
				routd( reg_addr, buf_addr, wc / 2 );
			}
			else
			{
				// do REP INSW
				routw( reg_addr, buf_addr, wc );
			}
			buf_addr = buf_addr + ( wc * 2 );
			word_cnt = word_cnt - wc;
		}
	}

   return;
}




