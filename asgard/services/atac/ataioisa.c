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
#include <services/dma_man/dma_man.h>	// for using dma channels

//***********************************************************
//
// Some notes about ISA bus DMA...
//
// ISA bus DMA uses an DMA controller built into an ISA bus
// motherboard.  This DMA controller has six DMA channels:  1, 2, 3,
// 5, 6 and 7. Channels 0 and 4 are reserved for other uses.
// Channels 1, 2 and 3 are 8-bit and channels 5, 6 and 7 are 16-bit.
// Since ATA DMA is always 16-bit only channels 5, 6 or 7 can be
// used here.
//
// An ISA bus DMA controller is unable to transfer data across a
// 128K boundary in physical memory.  
//
// Note that the ISA 16-bit DMA channels are restricted to
// transfering data on word boundaries and transfers of an even
// number of bytes.  This is because the host memory address and the
// transfer length byte count are both divided by 2. These word
// addresses and word counts are used by the DMA controller.
//
//***********************************************************

//***********************************************************
//
// set_up_xfer() -- set up for 1 or 2 dma transfers -- either
//                  1 or 2 transfers are required per ata
//                  or atapi command.
//
//***********************************************************

static void set_up_xfer( struct ata_channel *channel, int dir, long bc, unsigned int address );

static void set_up_xfer( struct ata_channel *channel, int dir, long bc, unsigned int address )
{
   channel->dma_count = (bc >> 1);

   // get dma channel mode
   channel->channel_mode = DEMAND_TRANSFER;      // change this line for single word dma
   channel->channel_mode = channel->channel_mode | ( dir ? READ : WRITE );
   channel->channel_mode = channel->channel_mode | channel->dma_channel;
}

//***********************************************************
//
// prog_dma_chan() -- config the dma channel we will use --
//                    we call this function to set each
//                    part of a dma transfer.
//
// 
//
//***********************************************************

static void prog_dma_chan( struct ata_channel *channel, unsigned long count, int mode );

static void prog_dma_chan( struct ata_channel *channel, unsigned long count, int mode )
{
	struct dma_response dma_res;
	struct dma_command prepare_dma = { SET_CHANNEL, ATAC_DMA_TRANSACT_PORT,
				     channel->dma_channel,
				     channel->channel_mode, 
				     0,	// hm.. if we are going to make this thing multithreaded this should be something meaningful, or use different ports
						// for each ata channel.. if not, place a mutex or something!
				     0,
				     count };
	int id;
	
	send_msg(DMA_MAN_TASK, DMA_COMMAND_PORT, &prepare_dma);

	while (get_msg_count(ATAC_DMA_TRANSACT_PORT) == 0) { reschedule(); }

	get_msg(ATAC_DMA_TRANSACT_PORT, &dma_res, &id);

	if (dma_res.result == DMA_OK) 
	{
		channel->dma_man_smo = dma_res.res1;   /* store the DMA buffer smo returned by the DMA manager */
	}
}

void isa_free_dma_channel(struct ata_channel *channel)
{
	struct dma_response dma_res;
	struct dma_command prepare_dma = { FREE_CHANNEL, ATAC_DMA_TRANSACT_PORT,
				     channel->dma_channel,
				     0, 
				     0,	// hm.. if we are going to make this thing multithreaded this should be something meaningful, or use different ports
						// for each ata channel.. if not, place a mutex or something!
				     0,
				     0 };
	int id;

	while (get_msg_count(ATAC_DMA_TRANSACT_PORT) == 0) { reschedule(); }

	get_msg(ATAC_DMA_TRANSACT_PORT, &dma_res, &id);
	channel->dma_man_smo = -1;
}

//***********************************************************
//
// chk_cmd_done() -- check for command completion
//
//***********************************************************

static int chk_cmd_done( struct ata_channel *channel );

static int chk_cmd_done( struct ata_channel *channel )
{
   int status;

   // check for interrupt or poll status

   if ( channel->int_use_intr_flag )
   {
      if ( channel->int_intr_flag )                // interrupt?
      {
		 status = channel->int_ata_status;         // get status
         return 1;                        // cmd done
      }
   }
   else
   {
      status = pio_inbyte( channel, CB_ASTAT );    // poll for not busy & DRQ/errors
      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
         return 1;                        // cmd done
      if ( ( status & ( CB_STAT_BSY | CB_STAT_DF ) ) == CB_STAT_DF )
         return 1;                        // cmd done
      if ( ( status & ( CB_STAT_BSY | CB_STAT_ERR ) ) == CB_STAT_ERR )
         return 1;                        // cmd done
   }
   return 0;                              // not done yet
}

//***********************************************************
//
// dma_isa_config() - configure/setup for Read/Write DMA
//
// The caller must call this function before attempting
// to use any ATA or ATAPI commands in ISA DMA mode.
//
//***********************************************************

int dma_isa_config( struct ata_channel *channel, int chan )

{

   // channel must be 0 (disable) or 5, 6 or 7.

   switch ( chan )
   {
      case 0:
         channel->dma_channel = 0; // disable ISA DMA
         return 0;
      case 5:                    // set up channel 5
         channel->dma_channel = 5;
         channel->dmaTCbit   = DMA_TC5;
         break;
      case 6:                    // set up channel 6
         channel->dma_channel = 6;
         channel->dmaTCbit   = DMA_TC6;
         break;
      case 7:                    // set up channel 7
         channel->dma_channel = 7;
         channel->dmaTCbit   = DMA_TC7;
         break;
      default:						// not channel 5, 6 or 7
         channel->dma_channel = 0;  // disable ISA DMA
         return 1;                  // return error
   }

   return 0;
}

//***********************************************************
//
// exec_isa_ata_cmd() - DMA in ISA Multiword for ATA R/W DMA
//
//***********************************************************

static int exec_isa_ata_cmd( struct ata_channel *channel, int dev,
                             unsigned int address,
                             long numSect );

static int exec_isa_ata_cmd( struct ata_channel *channel, int dev,
                             unsigned int address,
                             long numSect )
{
   unsigned char status;
   unsigned long lw1, lw2;

   // Quit now if the command is incorrect.

   if (    ( channel->reg_cmd_info.cmd != ATACMD_READ_DMA )
        && ( channel->reg_cmd_info.cmd != ATACMD_READ_DMA_EXT )
        && ( channel->reg_cmd_info.cmd != ATACMD_WRITE_DMA )
        && ( channel->reg_cmd_info.cmd != ATACMD_WRITE_DMA_EXT ) )
   {
      channel->reg_cmd_info.ec = 77;
      sub_trace_command(channel);
      return 1;
   }

   // Quit now if no dma channel set up.

   if ( ! channel->dma_channel )
   {
      channel->reg_cmd_info.ec = 70;
      sub_trace_command(channel);
      return 1;
   }

   // Quit now if 1) I/O buffer overrun possible
   // or 2) DMA can't handle the transfer size.
   if ( ( numSect > 256 ) || ( ( numSect * 512 ) > channel->reg_buffer_size ) )
   {
      channel->reg_cmd_info.ec = 61;
      sub_trace_command(channel);
      return 1;
   }

   // set up the dma transfer
   set_up_xfer( channel, ( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA )
                ||
                ( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA_EXT ),
                numSect * 512, address );

   // Set command time out.
   SIGNALHANDLER sigh = tmr_set_timeout(channel);

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( channel, dev ) )
   {
      sub_trace_command(channel);
	  tmr_discard_timeout(sigh);
      return 1;
   }

   // Set up all the registers except the command register.
   sub_setup_command(channel);

   // program the dma channel for the first or only transfer.
   prog_dma_chan( channel, channel->dma_count, channel->channel_mode );

   // if writing, write on the dma manager buffer 
   if( ( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA ) || ( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA_EXT ))
   {
		write_mem(channel->dma_man_smo, 0, (channel->dma_count << 1), (void*)address );
   }

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.
   pio_outbyte( channel, CB_CMD, channel->reg_cmd_info.cmd );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // Data transfer...
   // If this transfer requires two dma transfers,
   // wait for the first transfer to complete and
   // then program the dma channel for the second transfer.

   // wait for drive to signal command completion
   // -or-
   // time out if this takes to long.

   if ( channel->reg_cmd_info.ec == 0 )
   {
	  int done = 0;
      while ( tmr_check_timeout(sigh) == 0 )
      {
         if ( chk_cmd_done(channel) )               // cmd done ?
		 {
		    done = 1;
            break;                           // yes
		 }
      }
	  if ( !done )            // time out yet ?
      {
			channel->reg_cmd_info.to = 1;
			channel->reg_cmd_info.ec = 73;
    }
   }

   tmr_discard_timeout(sigh);

   if( !(( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA ) || ( channel->reg_cmd_info.cmd == ATACMD_WRITE_DMA_EXT )))
   {
		read_mem(channel->dma_man_smo, 0, (channel->dma_count << 1), (void*)address );
   }

   // End of command...
   // disable the dma channel
   isa_free_dma_channel(channel);

   // End of command...
   // If polling or error read the Status register,
   // otherwise use the Status register value that was read
   // by the interrupt handler.

   if ( ( ! channel->int_use_intr_flag ) || ( channel->reg_cmd_info.ec ) )
      status = pio_inbyte( channel, CB_STAT );
   else
      status = channel->int_ata_status;

   // Final status check...
   // if no error, check final status...
   // Error if BUSY, DEVICE FAULT, DRQ or ERROR status now.

   if ( channel->reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         channel->reg_cmd_info.ec = 74;
      }
   }

   // Final status check...
   // if no error, check dma channel terminal count flag
   if ( ( channel->reg_cmd_info.ec == 0 )
        &&
        ( ( inb( channel, 0xd0 ) & channel->dmaTCbit ) == 0 )
      )
   {
      channel->reg_cmd_info.ec = 71;
   }

   // Final status check...
   // update total bytes transferred

   if ( channel->reg_cmd_info.ec == 0 )
      channel->reg_cmd_info.totalBytesXfer += channel->dma_count << 1;

   // Done...
   // Read the output registers and trace the command.
   sub_trace_command(channel);

   if ( channel->reg_cmd_info.ec )
      return 1;
   return 0;
}

//***********************************************************
//
// dma_isa_chs() - DMA in ISA Multiword for ATA R/W DMA
//
//***********************************************************

int dma_isa_chs( struct ata_channel *channel, int dev, int cmd,
                     unsigned int fr, unsigned int sc,
                     unsigned int cyl, unsigned int head, unsigned int sect,
                     unsigned int address,
                     long numSect )

{

   // Setup current command information.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_ADMAI;
   if ( ( cmd == ATACMD_WRITE_DMA ) || ( cmd == ATACMD_WRITE_DMA_EXT ) )
      channel->reg_cmd_info.ct  = TRC_TYPE_ADMAO;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.sn1 = sect;
   channel->reg_cmd_info.cl1 = cyl & 0x00ff;
   channel->reg_cmd_info.ch1 = ( cyl & 0xff00 ) >> 8;
   channel->reg_cmd_info.dh1 = ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) | ( head & 0x0f );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = numSect;
   channel->reg_cmd_info.lbaSize = LBACHS;

   // Execute the command.
   return exec_isa_ata_cmd( channel, dev, address, numSect );
}

//***********************************************************
//
// dma_isa_lba28() - DMA in ISA Multiword for ATA R/W DMA
//
//***********************************************************

int dma_isa_lba28( struct ata_channel *channel, int dev, int cmd,
                       unsigned int fr, unsigned int sc,
                       unsigned long lba,
                       unsigned int address,
                       long numSect )

{

   // Setup current command information.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_ADMAI;
   channel->reg_cmd_info.cmd = cmd;
   if ( ( cmd == ATACMD_WRITE_DMA ) || ( cmd == ATACMD_WRITE_DMA_EXT ) )
      channel->reg_cmd_info.ct  = TRC_TYPE_ADMAO;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = numSect;
   channel->reg_cmd_info.lbaSize = LBA28;
   channel->reg_cmd_info.lbaHigh1 = 0L;
   channel->reg_cmd_info.lbaLow1 = lba;

   // Execute the command.

   return exec_isa_ata_cmd( channel, dev, address, numSect );
}

//***********************************************************
//
// dma_isa_lba48() - DMA in ISA Multiword for ATA R/W DMA
//
//***********************************************************

int dma_isa_lba48( struct ata_channel *channel, int dev, int cmd,
                       unsigned int fr, unsigned int sc,
                       unsigned long lbahi, unsigned long lbalo,
                       unsigned int address,
                       long numSect )

{

   // Setup current command information.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_ADMAI;
   if ( ( cmd == ATACMD_WRITE_DMA ) || ( cmd == ATACMD_WRITE_DMA_EXT ) )
      channel->reg_cmd_info.ct  = TRC_TYPE_ADMAO;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = numSect;
   channel->reg_cmd_info.lbaSize = LBA48;
   channel->reg_cmd_info.lbaHigh1 = lbahi;
   channel->reg_cmd_info.lbaLow1 = lbalo;

   // Execute the command.

   return exec_isa_ata_cmd( channel, dev, address, numSect );
}

//***********************************************************
//
// dma_isa_packet() - DMA in ISA Multiword for ATAPI Packet
//
//***********************************************************

int dma_isa_packet( struct ata_channel *channel, int dev,
                    unsigned int cpbc,
                    unsigned int cpaddress,
                    int dir,
                    long dpbc,
                    unsigned int dpaddress,
                    unsigned long lba )

{
   unsigned char status;
   unsigned char reason;
   unsigned char lowCyl;
   unsigned char highCyl;
   int ndx;
   unsigned char * cfp;
   unsigned long lw1, lw2;

   channel->int_intr_flag = 0;

   // Make sure the command packet size is either 12 or 16
   // and save the command packet size and data.

   cpbc = cpbc < 12 ? 12 : cpbc;
   cpbc = cpbc > 12 ? 16 : cpbc;

   // Setup current command information.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATAPI;
   channel->reg_cmd_info.ct  = dir ? TRC_TYPE_PDMAO : TRC_TYPE_PDMAI;
   channel->reg_cmd_info.cmd = ATACMD_PACKET;
   channel->reg_cmd_info.fr1 = channel->reg_atapi_reg_fr | 0x01;  // packet DMA mode !
   channel->reg_cmd_info.sc1 = channel->reg_atapi_reg_sc;
   channel->reg_cmd_info.sn1 = channel->reg_atapi_reg_sn;
   channel->reg_cmd_info.cl1 = 0;         // no Byte Count Limit in DMA !
   channel->reg_cmd_info.ch1 = 0;         // no Byte Count Limit in DMA !
   channel->reg_cmd_info.dh1 = dev ? CB_DH_DEV1 : CB_DH_DEV0;
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBA32;
   channel->reg_cmd_info.lbaLow1 = lba;
   channel->reg_cmd_info.lbaHigh1 = 0L;
   channel->reg_atapi_cp_size = cpbc;

   cfp = (unsigned char *)cpaddress;
   for ( ndx = 0; ndx < cpbc; ndx ++ )
   {
      channel->reg_atapi_cp_data[ndx] = *cfp;
      cfp ++ ;
   }

   // Zero the alternate ATAPI register data.
   channel->reg_atapi_reg_fr = 0;
   channel->reg_atapi_reg_sc = 0;
   channel->reg_atapi_reg_sn = 0;
   channel->reg_atapi_reg_dh = 0;

   // Quit now if no dma channel set up
   if ( ! channel->dma_channel )
   {
      channel->reg_cmd_info.ec = 70;
      sub_trace_command(channel);
      return 1;
   }

   // the data packet byte count must be even
   // and must not be zero

   if ( dpbc & 1L )
      dpbc ++ ;
   if ( dpbc < 2L )
      dpbc = 2L;

   // Quit now if 1) I/O buffer overrun possible
   // or 2) DMA can't handle the transfer size.

   if ( ( dpbc > 131072L ) || ( dpbc > channel->reg_buffer_size ) )
   {
      channel->reg_cmd_info.ec = 61;
      sub_trace_command(channel);
      return 1;
   }

   // set up the dma transfer(s)
   set_up_xfer( channel, dir, dpbc, cpaddress );

   // Set command time out.
   SIGNALHANDLER sigh = tmr_set_timeout(channel);

   // Select the drive - call the reg_select function.
   // Quit now if this fails.

   if ( sub_select( channel, dev ) )
   {
      sub_trace_command(channel);
      tmr_discard_timeout(sigh);
	  return 1;
   }

   // Set up all the registers except the command register.
   sub_setup_command(channel);

   // program the dma channel for the first or only transfer.
   prog_dma_chan(channel, channel->dma_count, channel->channel_mode );

   // if it's a write command, write on the dma man smo
   if( dir )
   {
		write_mem(channel->dma_man_smo, 0, (channel->dma_count << 1), (void*)cpaddress );
   }

   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.

   pio_outbyte( channel, CB_CMD, ATACMD_PACKET );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.

   DELAY400NS;

   // Command packet transfer...
   // Check for protocol failures,
   // the device should have BSY=1 or
   // if BSY=0 then either DRQ=1 or CHK=1.

   sub_atapi_delay( channel, dev );
   status = pio_inbyte( channel, CB_ASTAT );
   if ( status & CB_STAT_BSY )
   {
      // BSY=1 is OK
   }
   else
   {
      if ( status & ( CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         // BSY=0 and DRQ=1 is OK
         // BSY=0 and ERR=1 is OK
      }
      else
      {
         channel->reg_cmd_info.failbits |= FAILBIT0;  // not OK
      }
   }

   // Command packet transfer...
   // Poll Alternate Status for BSY=0.

   while ( tmr_check_timeout(sigh) == 0 )
   {
      status = pio_inbyte( channel, CB_ASTAT );       // poll for not busy
      if ( ( status & CB_STAT_BSY ) == 0 )
         break;
   }
   if ( ( status & CB_STAT_BSY ) != 0 )               // time out  ?
    {
        channel->reg_cmd_info.to = 1;
        channel->reg_cmd_info.ec = 75;
    }

   // Command packet transfer...
   // Check for protocol failures... no interrupt here please!
   // Clear any interrupt the command packet transfer may have caused.

   if ( channel->int_intr_flag )    // extra interrupt(s) ?
      channel->reg_cmd_info.failbits |= FAILBIT1;
   channel->int_intr_flag = 0;

   // Command packet transfer...
   // If no error, transfer the command packet.

   if ( channel->reg_cmd_info.ec == 0 )
   {

      // Command packet transfer...
      // Read the primary status register and the other ATAPI registers.
      status = pio_inbyte( channel, CB_STAT );
      reason = pio_inbyte( channel, CB_SC );
      lowCyl = pio_inbyte( channel, CB_CL );
      highCyl = pio_inbyte( channel, CB_CH );

      // Command packet transfer...
      // check status: must have BSY=0, DRQ=1 now

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) ) != CB_STAT_DRQ )
      {
         channel->reg_cmd_info.ec = 76;
      }
      else
      {
         // Command packet transfer...
         // Check for protocol failures...
         // check: C/nD=1, IO=0.

         if ( ( reason &  ( CB_SC_P_TAG | CB_SC_P_REL | CB_SC_P_IO ) )
              || ( ! ( reason &  CB_SC_P_CD ) )
            )
            channel->reg_cmd_info.failbits |= FAILBIT2;
         if (    ( lowCyl != channel->reg_cmd_info.cl1 )
              || ( highCyl != channel->reg_cmd_info.ch1 ) )
            channel->reg_cmd_info.failbits |= FAILBIT3;

         // Command packet transfer...
         // trace cdb byte 0,
         // xfer the command packet (the cdb)
         pio_drq_block_out( channel, CB_DATA, cpaddress, cpbc >> 1 );
      }
   }

   // Data transfer...
   // If this transfer requires two dma transfers,
   // wait for the first transfer to complete and
   // then program the dma channel for the second transfer.
   
   // wait for drive to signal command completion
   // -or-
   // time out if this takes to long.

   if ( channel->reg_cmd_info.ec == 0 )
   {
	  int done = 0;
      while ( tmr_check_timeout(sigh) == 0 )
      {
         if ( chk_cmd_done(channel) )               // cmd done ?
		 {
			done = 1;
            break;                           // yes
		 }
      }
	  if ( !done )            // time out yet ?
      {
		channel->reg_cmd_info.to = 1;
		channel->reg_cmd_info.ec = 73;
      } 
   }


   // End of command...
   // disable/stop the dma channel
   isa_free_dma_channel(channel);

   // End of command...
   // If polling or error read the Status register,
   // otherwise use the Status register value that was read
   // by the interrupt handler.

   if ( ( ! channel->int_use_intr_flag ) || ( channel->reg_cmd_info.ec ) )
      status = pio_inbyte( channel, CB_STAT );
   else
      status = channel->int_ata_status;

   // Final status check...
   // if no error, check final status...
   // Error if BUSY, DEVICE FAULT, DRQ or ERROR status now.

   if ( channel->reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         channel->reg_cmd_info.ec = 74;
      }
   }

   // Final status check...
   // Check for protocol failures...
   // check: C/nD=1, IO=1.
   reason = pio_inbyte( channel, CB_SC );
   if ( ( reason & ( CB_SC_P_TAG | CB_SC_P_REL ) )
        || ( ! ( reason & CB_SC_P_IO ) )
        || ( ! ( reason & CB_SC_P_CD ) )
      )
      channel->reg_cmd_info.failbits |= FAILBIT8;

   // Final status check...
   // update total bytes transferred
   channel->reg_cmd_info.totalBytesXfer += channel->dma_count;

   // Done...
   // Read the output registers and trace the command.

   sub_trace_command(channel);

   if ( channel->reg_cmd_info.ec )
      return 1;
   return 0;
}
