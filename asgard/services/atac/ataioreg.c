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


//*************************************************************
//
// reg_wait_poll() - wait for interrupt or poll for BSY=0
//
//*************************************************************

static void reg_wait_poll( struct ata_channel *channel, int waiterror, int pollerror  );

static void reg_wait_poll(struct ata_channel *channel, int waiterror, int pollerror )
{
	unsigned char status;

	// Wait for interrupt -or- wait for not BUSY -or- wait for time out.

	if ( waiterror && channel->int_use_intr_flag )
	{
		// wait for interrupt
		if(tmr_wait_timeout(channel))
		{
			// interrupt finished ok
			status = channel->int_ata_status;         // get status
		}
		else
		{
			// timeout, set the error
			channel->reg_cmd_info.to = 1;
			channel->reg_cmd_info.ec = waiterror;
		}
	}
	else
	{
		// wait polling
		SIGNALHANDLER sigh = tmr_set_timeout(channel);	// set a timeout signal

		while ( tmr_check_timeout(sigh) == 0)
		{
			status = pio_inbyte( channel, CB_ASTAT );       // poll for not busy
			if ( ( status & CB_STAT_BSY ) == 0 )
			{
				tmr_discard_timeout(sigh);
				return;
			}
		}

		tmr_discard_timeout(sigh);

		// timed out :( lets see if busy still active
		status = pio_inbyte( channel, CB_ASTAT );       // poll for not busy
		if ( ( status & CB_STAT_BSY ) == 0 )
				return;

		// ok ok... it failed
		channel->reg_cmd_info.to = 1;
		channel->reg_cmd_info.ec = pollerror;
	}
}

//*************************************************************
//
// reg_config() - Check the host adapter and determine the
//                number and type of drives attached.
//
// This process is not documented by any of the ATA standards.
//
// Infomation is returned by this function is in
// reg_config_info for each device -- see ATAIO.H.
//
//*************************************************************

int reg_config(struct ata_channel *channel )
{
   int num_dev = 0;
   unsigned char sc;
   unsigned char sn;
   unsigned char cl;
   unsigned char ch;
   unsigned char st;
   unsigned char dev_ctrl;

   // setup register values
   dev_ctrl = CB_DC_HD15 | ( channel->int_use_intr_flag ? 0 : CB_DC_NIEN );

   // assume there are no devices
   channel->devices[0].reg_config_info = REG_CONFIG_TYPE_NONE;
   channel->devices[1].reg_config_info = REG_CONFIG_TYPE_NONE;

   // set up Device Control register
   pio_outbyte( channel, CB_DC, dev_ctrl );

   // lets see if there is a device 0
   pio_outbyte( channel, CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   pio_outbyte( channel, CB_SC, 0x55 );
   pio_outbyte( channel, CB_SN, 0xaa );
   pio_outbyte( channel, CB_SC, 0xaa );
   pio_outbyte( channel, CB_SN, 0x55 );
   pio_outbyte( channel, CB_SC, 0x55 );
   pio_outbyte( channel, CB_SN, 0xaa );
   sc = pio_inbyte( channel, CB_SC );
   sn = pio_inbyte( channel, CB_SN );
   if ( ( sc == 0x55 ) && ( sn == 0xaa ) )
      channel->devices[0].reg_config_info = REG_CONFIG_TYPE_UNKN;

   // lets see if there is a device 1
   pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
   DELAY400NS;
   pio_outbyte( channel, CB_SC, 0x55 );
   pio_outbyte( channel, CB_SN, 0xaa );
   pio_outbyte( channel, CB_SC, 0xaa );
   pio_outbyte( channel, CB_SN, 0x55 );
   pio_outbyte( channel, CB_SC, 0x55 );
   pio_outbyte( channel, CB_SN, 0xaa );
   sc = pio_inbyte( channel, CB_SC );
   sn = pio_inbyte( channel, CB_SN );

   if ( ( sc == 0x55 ) && ( sn == 0xaa ) )
       channel->devices[1].reg_config_info = REG_CONFIG_TYPE_UNKN;

   // now we think we know which devices, if any are there,
   // so lets try a soft reset (ignoring any errors).
   pio_outbyte( channel, CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   reg_reset( channel, 0, 0 );

   // lets check device 0 again, is device 0 really there?
   // is it ATA or ATAPI?
   pio_outbyte( channel, CB_DH, CB_DH_DEV0 );
   DELAY400NS;
   sc = pio_inbyte( channel, CB_SC );
   sn = pio_inbyte( channel, CB_SN );
   if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
   {
      channel->devices[0].reg_config_info = REG_CONFIG_TYPE_UNKN;
      st = pio_inbyte( channel, CB_STAT );
      cl = pio_inbyte( channel, CB_CL );
      ch = pio_inbyte( channel, CB_CH );
      if ( ( ( cl == 0x14 ) && ( ch == 0xeb ) )       // PATAPI
           ||
           ( ( cl == 0x69 ) && ( ch == 0x96 ) )       // SATAPI
         )
      {
         channel->devices[0].reg_config_info = REG_CONFIG_TYPE_ATAPI;
      }
      else
      if ( ( st != 0 )
           &&
           ( ( ( cl == 0x00 ) && ( ch == 0x00 ) )     // PATA
             ||
             ( ( cl == 0x3c ) && ( ch == 0xc3 ) ) )   // SATA
         )
      {
         channel->devices[0].reg_config_info = REG_CONFIG_TYPE_ATA;
      }
   }

   // lets check device 1 again, is device 1 really there?
   // is it ATA or ATAPI?
   pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
   DELAY400NS;
   sc = pio_inbyte( channel, CB_SC );
   sn = pio_inbyte( channel, CB_SN );
   if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
   {
      channel->devices[1].reg_config_info = REG_CONFIG_TYPE_UNKN;
      st = pio_inbyte( channel, CB_STAT );
      cl = pio_inbyte( channel, CB_CL );
      ch = pio_inbyte( channel, CB_CH );
      if ( ( ( cl == 0x14 ) && ( ch == 0xeb ) )       // PATAPI
           ||
           ( ( cl == 0x69 ) && ( ch == 0x96 ) )       // SATAPI
         )
      {
         channel->devices[1].reg_config_info = REG_CONFIG_TYPE_ATAPI;
      }
      else
      if ( ( st != 0 )
           &&
           ( ( ( cl == 0x00 ) && ( ch == 0x00 ) )     // PATA
             ||
             ( ( cl == 0x3c ) && ( ch == 0xc3 ) ) )   // SATA
         )
      {
         channel->devices[1].reg_config_info = REG_CONFIG_TYPE_ATA;
      }
   }

   // If possible, select a device that exists, try device 0 first.
   if ( channel->devices[1].reg_config_info != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
      DELAY400NS;
      num_dev ++ ;
   }
   if ( channel->devices[0].reg_config_info != REG_CONFIG_TYPE_NONE )
   {
      pio_outbyte( channel, CB_DH, CB_DH_DEV0 );
      DELAY400NS;
	  num_dev ++ ;
   }

   // return the number of devices found
   return num_dev;
}

//*************************************************************
//
// reg_reset() - Execute a Software Reset.
//
// See ATA-2 Section 9.2, ATA-3 Section 9.2, ATA-4 Section 8.3.
//
//*************************************************************

int reg_reset(struct ata_channel *channel, int skip_flag, int dev_ret )
{
	unsigned char sc;
	unsigned char sn;
	unsigned char status;
	unsigned char dev_ctrl;
	int timedout;

	// setup register values
	dev_ctrl = CB_DC_HD15 | ( channel->int_use_intr_flag ? 0 : CB_DC_NIEN );

	// Reset error return data.
	sub_zero_return_data(channel);

	channel->reg_cmd_info.flg = TRC_FLAG_SRST;
	channel->reg_cmd_info.ct  = TRC_TYPE_ASR;

	// initialize the command timeout counter
	SIGNALHANDLER sigh = tmr_set_timeout(channel);

	// Set and then reset the soft reset bit in the Device Control
	// register.  This causes device 0 be selected.
	if ( ! skip_flag )
	{
		pio_outbyte( channel, CB_DC, dev_ctrl | CB_DC_SRST );
		DELAY400NS;
		pio_outbyte( channel, CB_DC, dev_ctrl );
		DELAY400NS;
	}

	timedout = 0;
	// If there is a device 0, wait for device 0 to set BSY=0.
	if ( channel->devices[0].reg_config_info != REG_CONFIG_TYPE_NONE )
	{
		sub_atapi_delay( channel, 0 );

		while ( tmr_check_timeout(sigh) == 0 )
		{
			status = pio_inbyte( channel, CB_STAT );
			if ( ( status & CB_STAT_BSY ) == 0 )
			{
				break;
			}
		}
		if(( status & CB_STAT_BSY ) != 0)
		{
			// timeout
			channel->reg_cmd_info.to = 1;
			channel->reg_cmd_info.ec = 1;
			timedout = 1;
		}
	}

	if(timedout) 
	{
		tmr_discard_timeout(sigh);
		sigh = tmr_set_timeout(channel);
	}

	// If there is a device 1, wait until device 1 allows
	// register access.
	timedout = 0;
	if ( channel->devices[1].reg_config_info != REG_CONFIG_TYPE_NONE )
	{
		sub_atapi_delay( channel, 1 );

		while ( tmr_check_timeout(sigh) == 0 )
		{
			pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
			DELAY400NS;
			sc = pio_inbyte( channel, CB_SC );
			sn = pio_inbyte( channel, CB_SN );
			if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
			{
				break;
			}
		}
		if(!( ( sc == 0x01 ) && ( sn == 0x01 ) ))
		{
			// timeout
			channel->reg_cmd_info.to = 1;
			channel->reg_cmd_info.ec = 1;
			timedout = 1;
		}

		// Now check if drive 1 set BSY=0.

		if ( channel->reg_cmd_info.ec == 0 )
		{
			if ( pio_inbyte( channel, CB_STAT ) & CB_STAT_BSY )
			{
				channel->reg_cmd_info.ec = 3;
			}
		}
	}

	tmr_discard_timeout(sigh);

	// RESET_DONE:

	// We are done but now we must select the device the caller
	// requested before we trace the command.  This will cause
	// the correct data to be returned in reg_cmd_info.

	pio_outbyte( channel, CB_DH, dev_ret ? CB_DH_DEV1 : CB_DH_DEV0 );
	DELAY400NS;
	sub_trace_command(channel);

	// If possible, select a device that exists,
	// try device 0 first.

	if ( channel->devices[0].reg_config_info != REG_CONFIG_TYPE_NONE )
	{
		pio_outbyte( channel, CB_DH, CB_DH_DEV0 );
		DELAY400NS;
	}
	if ( channel->devices[1].reg_config_info != REG_CONFIG_TYPE_NONE )
	{
		pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
		DELAY400NS;
	}

	// All done.  The return values of this function are described in
	// ATAIO.H.

	if ( channel->reg_cmd_info.ec )
		return 1;
	return 0;
}

//*************************************************************
//
// exec_non_data_cmd() - Execute a non-data command.
//
// Note special handling for Execute Device Diagnostics
// command when there is no device 0.
//
// See ATA-2 Section 9.5, ATA-3 Section 9.5,
// ATA-4 Section 8.8 Figure 12.  Also see Section 8.5.
//
//*************************************************************

static int exec_non_data_cmd( struct ata_channel *channel, int dev );

static int exec_non_data_cmd( struct ata_channel *channel, int dev )
{
   unsigned char sec_cnt;
   unsigned char sec_num;
   unsigned char status;
   int polled = 0;
  
   // Set command time out.
   SIGNALHANDLER sigh = tmr_set_timeout(channel);

   // PAY ATTENTION HERE
   // If the caller is attempting a Device Reset command, then
   // don't do most of the normal stuff.  Device Reset has no
   // parameters, should not generate an interrupt and it is the
   // only command that can be written to the Command register
   // when a device has BSY=1 (a very strange command!).  Not
   // all devices support this command (even some ATAPI devices
   // don't support the command.

   if ( channel->reg_cmd_info.cmd != ATACMD_DEVICE_RESET )
   {
      // Select the drive - call the sub_select function.
      // Quit now if this fails.
      if ( sub_select( channel, dev ) )
      {
         sub_trace_command(channel);
         return 1;
      }

      // Set up all the registers except the command register.
      sub_setup_command(channel);
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

   if ( channel->devices[0].reg_config_info == REG_CONFIG_TYPE_ATAPI )
      sub_atapi_delay( channel, 0 );
   if ( channel->devices[1].reg_config_info == REG_CONFIG_TYPE_ATAPI )
      sub_atapi_delay( channel, 1 );

   // IF
   //    This is an Exec Dev Diag command (cmd=0x90)
   //    and there is no device 0 then
   //    there will be no interrupt. So we must
   //    poll device 1 until it allows register
   //    access and then do normal polling of the Status
   //    register for BSY=0.
   // ELSE
   // IF
   //    This is a Dev Reset command (cmd=0x08) then
   //    there should be no interrupt.  So we must
   //    poll for BSY=0.
   // ELSE
   //    Do the normal wait for interrupt or polling for
   //    completion.

   if ( ( channel->reg_cmd_info.cmd == ATACMD_EXECUTE_DEVICE_DIAGNOSTIC )
        &&
        ( channel->devices[0].reg_config_info == REG_CONFIG_TYPE_NONE )
      )
   {
		polled = 1;

		while ( tmr_check_timeout(sigh) == 0 )
		{
			pio_outbyte( channel, CB_DH, CB_DH_DEV1 );
			DELAY400NS;
			sec_cnt = pio_inbyte( channel, CB_SC );
			sec_num = pio_inbyte( channel, CB_SN );
			if ( ( sec_cnt == 0x01 ) && ( sec_num == 0x01 ) )
				break;
		}
		if ( !(( sec_cnt == 0x01 ) && ( sec_num == 0x01 )) )
        {
           channel->reg_cmd_info.to = 1;
           channel->reg_cmd_info.ec = 24;
        }
   }
   else
   {
      if ( channel->reg_cmd_info.cmd == ATACMD_DEVICE_RESET )
      {
         // Wait for not BUSY -or- wait for time out.
         polled = 1;
         reg_wait_poll( channel, 0, 23 );
      }
      else
      {
         // Wait for interrupt -or- wait for not BUSY -or- wait for time out.

         if ( ! channel->int_use_intr_flag )
            polled = 1;
         reg_wait_poll( channel, 22, 23 );
      }
   }

   // If status was polled or if any error read the status register,
   // otherwise get the status that was read by the interrupt handler.
   if ( ( polled ) || ( channel->reg_cmd_info.ec ) )
      status = pio_inbyte( channel, CB_STAT );
   else
      status = channel->int_ata_status;

   // Error if BUSY, DEVICE FAULT, DRQ or ERROR status now.

   if ( channel->reg_cmd_info.ec == 0 )
   {
      if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
			channel->reg_cmd_info.ec = 21;
      }
   }

   // read the output registers and trace the command.
   sub_trace_command(channel);

   tmr_discard_timeout(sigh);

   if ( channel->reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// reg_non_data_chs() - Execute a non-data command.
//
// Note special handling for Execute Device Diagnostics
// command when there is no device 0.
//
// See ATA-2 Section 9.5, ATA-3 Section 9.5,
// ATA-4 Section 8.8 Figure 12.  Also see Section 8.5.
//
//*************************************************************

int reg_non_data_chs( struct ata_channel *channel, int dev, int cmd,
                      unsigned short fr, unsigned short sc,
                      unsigned short cyl, unsigned short head, unsigned short sect )
{

   // Setup command parameters.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_AND;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.sn1 = sect;
   channel->reg_cmd_info.cl1 = cyl & 0x00ff;
   channel->reg_cmd_info.ch1 = ( cyl & 0xff00 ) >> 8;
   channel->reg_cmd_info.dh1 = ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) | ( head & 0x0f );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = sc;
   channel->reg_cmd_info.lbaSize = LBACHS;

   // Execute the command.

   return exec_non_data_cmd( channel, dev );
}

//*************************************************************
//
// reg_non_data_lba28() - Easy way to execute a non-data command
//                        using an LBA sector address.
//
//*************************************************************

int reg_non_data_lba28( struct ata_channel *channel, int dev, int cmd,
                        unsigned short fr, unsigned short sc,
                        unsigned long lba )

{

   // Setup current command information.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_AND;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = sc;
   channel->reg_cmd_info.lbaSize = LBA28;
   channel->reg_cmd_info.lbaHigh1 = 0;
   channel->reg_cmd_info.lbaLow1 = lba;

   // Execute the command.

   return exec_non_data_cmd( channel, dev );
}

//*************************************************************
//
// reg_non_data_lba48() - Easy way to execute a non-data command
//                        using an LBA sector address.
//
//*************************************************************

int reg_non_data_lba48( struct ata_channel *channel,int dev, int cmd,
                        unsigned short fr, unsigned short sc,
                        unsigned long lbahi, unsigned long lbalo )

{

   // Setup current command infomation.

   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_AND;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.ns  = sc;
   channel->reg_cmd_info.lbaSize = LBA48;
   channel->reg_cmd_info.lbaHigh1 = lbahi;
   channel->reg_cmd_info.lbaLow1 = lbalo;

   // Execute the command.

   return exec_non_data_cmd( channel, dev );
}

//*************************************************************
//
// exec_pio_data_in_cmd() - Execute a PIO Data In command.
//
// See ATA-2 Section 9.3, ATA-3 Section 9.3,
// ATA-4 Section 8.6 Figure 10.
//
//*************************************************************

static int exec_pio_data_in_cmd( struct ata_channel *channel, int dev,
                            unsigned int address,
                            long num_sect, int multi_cnt );


static int exec_pio_data_in_cmd( struct ata_channel *channel, int dev,
                            unsigned int address,
                            long num_sect, int multi_cnt )

{
	unsigned char status;
	long word_cnt;
	unsigned int save_addr = address;

	// Select the drive - call the sub_select function.
	// Quit now if this fails.
	if ( sub_select( channel, dev ) )
	{
		sub_trace_command(channel);
		channel->reg_drq_block_call_back = (void *) 0;
		return 1;
	}

	// Set up all the registers except the command register.
	sub_setup_command(channel);

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

	// Loop to read each sector.
	while ( 1 )
	{
		// READ_LOOP:
		//
		// NOTE NOTE NOTE ...  The primary status register (1f7) MUST NOT be
		// read more than ONCE for each sector transferred!  When the
		// primary status register is read, the drive resets IRQ 14.  The
		// alternate status register (3f6) can be read any number of times.
		// After interrupt read the the primary status register ONCE
		// and transfer the 256 words (REP INSW).  AS SOON as BOTH the
		// primary status register has been read AND the last of the 256
		// words has been read, the drive is allowed to generate the next
		// IRQ 14 (newer and faster drives could generate the next IRQ 14 in
		// 50 microseconds or less).  If the primary status register is read
		// more than once, there is the possibility of a race between the
		// drive and the software and the next IRQ 14 could be reset before
		// the system interrupt controller sees it.

		// Wait for interrupt -or- wait for not BUSY -or- wait for time out.
		sub_atapi_delay( channel, dev );
		reg_wait_poll( channel, 34, 35 );

		// If polling or error read the status, otherwise
		// get the status that was read by the interrupt handler.
		if ( ( ! channel->int_use_intr_flag ) || ( channel->reg_cmd_info.ec ) )
			status = pio_inbyte( channel, CB_STAT );
		else
			status = channel->int_ata_status;

		// If there was a time out error, go to READ_DONE.
		if ( channel->reg_cmd_info.ec )
		{
			break;   // go to READ_DONE
		}

		// If BSY=0 and DRQ=1, transfer the data,
		// even if we find out there is an error later.
		if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
		{
			// do the slow data transfer thing
			if ( channel->reg_slow_xfer_flag )
			{
				if ( num_sect <= channel->reg_slow_xfer_flag )
				{
					sub_xfer_delay(channel);
					channel->reg_slow_xfer_flag = 0;
				}
			}

			// increment number of DRQ packets
			channel->reg_cmd_info.drqPackets++;

			// determine the number of sectors to transfer
			word_cnt = multi_cnt ? multi_cnt : 1;
			if ( word_cnt > num_sect )
				word_cnt = num_sect;
			word_cnt = word_cnt * 256;

			// Quit if buffer overrun.
			// Adjust buffer address when DRQ block call back in use.
			if ( channel->reg_drq_block_call_back )
			{
				if ( ( word_cnt << 1 ) > channel->reg_buffer_size )
				{
					channel->reg_cmd_info.ec = 61;
					break;   // go to READ_DONE
				}
				address = save_addr;
			}
			else
			{
				if ( ( channel->reg_cmd_info.totalBytesXfer + ( word_cnt << 1 ) ) > channel->reg_buffer_size )
				{
					channel->reg_cmd_info.ec = 61;
					break;   // go to READ_DONE
				}
			}

			// Do the REP INSW to read the data for one DRQ block.
			channel->reg_cmd_info.totalBytesXfer += ( word_cnt << 1 );
			pio_drq_block_in( channel, CB_DATA, address, word_cnt );

			DELAY400NS;    // delay so device can get the status updated

			// Note: The drive should have dropped DATA REQUEST by now.  If there
			// are more sectors to transfer, BUSY should be active now (unless
			// there is an error).

			// Call DRQ block call back function.
			if ( channel->reg_drq_block_call_back )
			{
				channel->reg_cmd_info.drqPacketSize = ( word_cnt << 1 );
				(* channel->reg_drq_block_call_back) ( channel, &channel->reg_cmd_info );
			}

			// Decrement the count of sectors to be transferred
			// and increment buffer address.
			num_sect = num_sect - ( multi_cnt ? multi_cnt : 1 );
			address = address + ( 512 * ( multi_cnt ? multi_cnt : 1 ) );
		}

		// So was there any error condition?
		if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR ) )
		{
			channel->reg_cmd_info.ec = 31;
			break;   // go to READ_DONE
		}

		// DRQ should have been set -- was it?
		if ( ( status & CB_STAT_DRQ ) == 0 )
		{
			channel->reg_cmd_info.ec = 32;
			break;   // go to READ_DONE
		}

		// If all of the requested sectors have been transferred, make a
		// few more checks before we exit.

		if ( num_sect < 1 )
		{
			// Since the drive has transferred all of the requested sectors
			// without error, the drive should not have BUSY, DEVICE FAULT,
			// DATA REQUEST or ERROR active now.
			sub_atapi_delay( channel, dev );
			status = pio_inbyte( channel, CB_STAT );

			if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
			{
				channel->reg_cmd_info.ec = 33;	
				break;   // go to READ_DONE
			}

			// All sectors have been read without error, go to READ_DONE.
			break;   // go to READ_DONE
		}

		// This is the end of the read loop.  If we get here, the loop is
		// repeated to read the next sector.  Go back to READ_LOOP.
	}

	// read the output registers and trace the command.
	sub_trace_command(channel);

	// READ_DONE:

	// reset reg_drq_block_call_back to NULL (0)

	channel->reg_drq_block_call_back = (void *) 0;

	// All done.  The return values of this function are described in
	// ATAIO.H.

	if ( channel->reg_cmd_info.ec )
		return 1;
	return 0;
}

//*************************************************************
//
// reg_pio_data_in_chs() - Execute a PIO Data In command.
//
// See ATA-2 Section 9.3, ATA-3 Section 9.3,
// ATA-4 Section 8.6 Figure 10.
//
//*************************************************************

int reg_pio_data_in_chs( struct ata_channel *channel, int dev, int cmd,
                         unsigned short fr, unsigned short sc,
                         unsigned short cyl, unsigned short head, unsigned short sect,
                         unsigned int address,
                         long num_sect, int multi_cnt )

{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDI;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.sn1 = sect;
   channel->reg_cmd_info.cl1 = cyl & 0x00ff;
   channel->reg_cmd_info.ch1 = ( cyl & 0xff00 ) >> 8;
   channel->reg_cmd_info.dh1 = ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) | ( head & 0x0f );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBACHS;

   // these commands transfer only 1 sector
   if (    ( cmd == ATACMD_IDENTIFY_DEVICE )
        || ( cmd == ATACMD_IDENTIFY_DEVICE_PACKET )
      )
      num_sect = 1;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Read Multiple uses multi_cnt
      if ( cmd != ATACMD_READ_MULTIPLE )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_in_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// reg_pio_data_in_lba28() - Easy way to execute a PIO Data In command
//                           using an LBA sector address.
//
//*************************************************************

int reg_pio_data_in_lba28( struct ata_channel *channel, int dev, int cmd,
                           unsigned short fr, unsigned short sc,
                           unsigned long lba,
                           unsigned int address,
                           long num_sect, int multi_cnt )

{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDI;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBA28;
   channel->reg_cmd_info.lbaHigh1 = 0;
   channel->reg_cmd_info.lbaLow1 = lba;

   // these commands transfer only 1 sector
   if (    ( cmd == ATACMD_IDENTIFY_DEVICE )
        || ( cmd == ATACMD_IDENTIFY_DEVICE_PACKET )
      )
      num_sect = 1;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Read Multiple uses multi_cnt
      if ( cmd != ATACMD_READ_MULTIPLE )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_in_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// reg_pio_data_in_lba48() - Easy way to execute a PIO Data In command
//                           using an LBA sector address.
//
//*************************************************************

int reg_pio_data_in_lba48( struct ata_channel *channel, int dev, int cmd,
                           unsigned short fr, unsigned short sc,
                           unsigned long lbahi, unsigned long lbalo,
                           unsigned int address,
                           long num_sect, int multi_cnt )
{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDI;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBA48;
   channel->reg_cmd_info.lbaHigh1 = lbahi;
   channel->reg_cmd_info.lbaLow1 = lbalo;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Read Multiple Ext uses multi_cnt
      if ( cmd != ATACMD_READ_MULTIPLE_EXT )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_in_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// exec_pio_data_out_cmd() - Execute a PIO Data Out command.
//
// See ATA-2 Section 9.4, ATA-3 Section 9.4,
// ATA-4 Section 8.7 Figure 11.
//
//*************************************************************

static int exec_pio_data_out_cmd(  struct ata_channel *channel, int dev,
                             unsigned int address,
                             long num_sect, int multi_cnt );

static int exec_pio_data_out_cmd( struct ata_channel *channel, int dev,
                             unsigned int address,
                             long num_sect, int multi_cnt )

{
	unsigned char status;
	int loop_flag = 1;
	long word_cnt;
	unsigned int save_addr = address;

	// Set command time out.
	SIGNALHANDLER sigh = tmr_set_timeout(channel);

	channel->int_intr_flag = 0;

	// Select the drive - call the sub_select function.
	// Quit now if this fails.

	if ( sub_select( channel, dev ) )
	{
		sub_trace_command(channel);
		channel->reg_drq_block_call_back = (void *) 0;
		tmr_discard_timeout(sigh);
		return 1;
	}
				
	// Set up all the registers except the command register.
	sub_setup_command(channel);

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

	// Wait for not BUSY or time out.
	// Note: No interrupt is generated for the
	// first sector of a write command.  Well...
	// that's not really true we are working with
	// a PCMCIA PC Card ATA device.

	sub_atapi_delay( channel, dev );
	
	while ( tmr_check_timeout(sigh) == 0 )
	{
		status = pio_inbyte( channel, CB_ASTAT );
		if ( ( status & CB_STAT_BSY ) == 0 )
			break;
	}
	if ( ( status & CB_STAT_BSY ) != 0 )
	{
		channel->reg_cmd_info.to = 1;
		channel->reg_cmd_info.ec = 47;
		loop_flag = 0;
	}

	// If we are using interrupts and we just got an interrupt, this is
	// wrong.  The drive must not generate an interrupt at this time.
	if ( loop_flag && channel->int_use_intr_flag && channel->int_intr_flag )
	{
		channel->reg_cmd_info.ec = 46;
		loop_flag = 0;
	}

	// This loop writes each sector.

	while ( loop_flag )
	{
		// WRITE_LOOP:
		//
		// NOTE NOTE NOTE ...  The primary status register (1f7) MUST NOT be
		// read more than ONCE for each sector transferred!  When the
		// primary status register is read, the drive resets IRQ 14.  The
		// alternate status register (3f6) can be read any number of times.
		// For correct results, transfer the 256 words (REP OUTSW), wait for
		// interrupt and then read the primary status register.  AS
		// SOON as BOTH the primary status register has been read AND the
		// last of the 256 words has been written, the drive is allowed to
		// generate the next IRQ 14 (newer and faster drives could generate
		// the next IRQ 14 in 50 microseconds or less).  If the primary
		// status register is read more than once, there is the possibility
		// of a race between the drive and the software and the next IRQ 14
		// could be reset before the system interrupt controller sees it.

		// If BSY=0 and DRQ=1, transfer the data,
		// even if we find out there is an error later.

		if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
		{
			// do the slow data transfer thing

			if ( channel->reg_slow_xfer_flag )
			{
				if ( num_sect <= channel->reg_slow_xfer_flag )
				{
					sub_xfer_delay(channel);
					channel->reg_slow_xfer_flag = 0;
				}
			}

			// increment number of DRQ packets
			channel->reg_cmd_info.drqPackets ++ ;

			// determine the number of sectors to transfer
			word_cnt = multi_cnt ? multi_cnt : 1;
			if ( word_cnt > num_sect )
			word_cnt = num_sect;
			word_cnt = word_cnt * 256;

			// Quit if buffer overrun.
			// If DRQ call back in use:
			// a) Call DRQ block call back function.
			// b) Adjust buffer address.
			if ( channel->reg_drq_block_call_back )
			{
				if ( ( word_cnt << 1 ) > channel->reg_buffer_size )
				{
					channel->reg_cmd_info.ec = 61;
					break;   // go to READ_DONE
				}
				channel->reg_cmd_info.drqPacketSize = ( word_cnt << 1 );
				(* channel->reg_drq_block_call_back) ( channel, &channel->reg_cmd_info );
				
				address = save_addr;
			}
			else
			{
				if ( ( channel->reg_cmd_info.totalBytesXfer + ( word_cnt << 1 ) ) > channel->reg_buffer_size )
				{
					channel->reg_cmd_info.ec = 61;
					break;   // go to READ_DONE
				}
			}

			// Do the REP OUTSW to write the data for one DRQ block.
			channel->reg_cmd_info.totalBytesXfer += ( word_cnt << 1 );
			pio_drq_block_out( channel, CB_DATA, address, word_cnt );

			DELAY400NS;    // delay so device can get the status updated

			// Note: The drive should have dropped DATA REQUEST and
			// raised BUSY by now.

			// Decrement the count of sectors to be transferred
			// and increment buffer address.

			num_sect = num_sect - ( multi_cnt ? multi_cnt : 1 );
			address = address + ( 512 * ( multi_cnt ? multi_cnt : 1 ) );
		}

		// So was there any error condition?

		if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR ) )
		{
			channel->reg_cmd_info.ec = 41;
			break;   // go to WRITE_DONE
		}

		// DRQ should have been set -- was it?

		if ( ( status & CB_STAT_DRQ ) == 0 )
		{
			channel->reg_cmd_info.ec = 42;
			break;   // go to WRITE_DONE
		}

		// Wait for interrupt -or- wait for not BUSY -or- wait for time out.
		sub_atapi_delay( channel, dev );
		reg_wait_poll( channel, 44, 45 );

		// If polling or error read the status, otherwise
		// get the status that was read by the interrupt handler.

		if ( ( ! channel->int_use_intr_flag ) || ( channel->reg_cmd_info.ec ) )
			status = pio_inbyte( channel, CB_STAT );
		else
			status = channel->int_ata_status;

		// If there was a time out error, go to WRITE_DONE.

		if ( channel->reg_cmd_info.ec )
			break;   // go to WRITE_DONE

		// If all of the requested sectors have been transferred, make a
		// few more checks before we exit.

		if ( num_sect < 1 )
		{
			// Since the drive has transferred all of the sectors without
			// error, the drive MUST not have BUSY, DEVICE FAULT, DATA REQUEST
			// or ERROR status at this time.

			if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
			{
				channel->reg_cmd_info.ec = 43;
				break;   // go to WRITE_DONE
			}

			// All sectors have been written without error, go to WRITE_DONE.

			break;   // go to WRITE_DONE

		}

		//
		// This is the end of the write loop.  If we get here, the loop
		// is repeated to write the next sector.  Go back to WRITE_LOOP.

	}

	// read the output registers and trace the command.
	sub_trace_command(channel);

	// WRITE_DONE:

	// reset reg_drq_block_call_back to NULL (0)
	channel->reg_drq_block_call_back = (void *) 0;

	// All done.  The return values of this function are described in
	// ATAIO.H.
	tmr_discard_timeout(sigh);

	if ( channel->reg_cmd_info.ec )
		return 1;
	return 0;
}

//*************************************************************
//
// reg_pio_data_out_chs() - Execute a PIO Data Out command.
//
// See ATA-2 Section 9.4, ATA-3 Section 9.4,
// ATA-4 Section 8.7 Figure 11.
//
//*************************************************************

int reg_pio_data_out_chs( struct ata_channel *channel, int dev, int cmd,
                          unsigned short fr, unsigned short sc,
                          unsigned short cyl, unsigned short head, unsigned short sect,
                          unsigned int address,
                          long num_sect, int multi_cnt )

{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDO;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.sn1 = sect;
   channel->reg_cmd_info.cl1 = cyl & 0x00ff;
   channel->reg_cmd_info.ch1 = ( cyl & 0xff00 ) >> 8;
   channel->reg_cmd_info.dh1 = ( dev ? CB_DH_DEV1 : CB_DH_DEV0 ) | ( head & 0x0f );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBACHS;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Write Multiple and CFA Write Multiple W/O Erase uses multi_cnt
      if (    ( cmd != ATACMD_WRITE_MULTIPLE )
           && ( cmd != ATACMD_CFA_WRITE_MULTIPLE_WO_ERASE )
         )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_out_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// reg_pio_data_out_lba28() - Easy way to execute a PIO Data In command
//                            using an LBA sector address.
//
//*************************************************************

int reg_pio_data_out_lba28( struct ata_channel *channel, int dev, int cmd,
                            unsigned short fr, unsigned short sc,
                            unsigned long lba,
                            unsigned int address,
                            long num_sect, int multi_cnt )

{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDO;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBA28;
   channel->reg_cmd_info.lbaHigh1 = 0;
   channel->reg_cmd_info.lbaLow1 = lba;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Write Multiple and CFA Write Multiple W/O Erase uses multi_cnt
      if (    ( cmd != ATACMD_WRITE_MULTIPLE )
           && ( cmd != ATACMD_CFA_WRITE_MULTIPLE_WO_ERASE )
         )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_out_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// reg_pio_data_out_lba48() - Easy way to execute a PIO Data In command
//                            using an LBA sector address.
//
//*************************************************************

int reg_pio_data_out_lba48( struct ata_channel *channel, int dev, int cmd,
                            unsigned short fr, unsigned short sc,
                            unsigned long lbahi, unsigned long lbalo,
                            unsigned int address,
                            long num_sect, int multi_cnt )
{

   // Reset error return data.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATA;
   channel->reg_cmd_info.ct  = TRC_TYPE_APDO;
   channel->reg_cmd_info.cmd = cmd;
   channel->reg_cmd_info.fr1 = fr;
   channel->reg_cmd_info.sc1 = sc;
   channel->reg_cmd_info.dh1 = CB_DH_LBA | (dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   channel->reg_cmd_info.dc1 = channel->int_use_intr_flag ? 0 : CB_DC_NIEN;
   channel->reg_cmd_info.lbaSize = LBA48;
   channel->reg_cmd_info.lbaHigh1 = lbahi;
   channel->reg_cmd_info.lbaLow1 = lbalo;

   // adjust multiple count
   if ( multi_cnt & 0x0800 )
   {
      // assume caller knows what they are doing
      multi_cnt &= 0x00ff;
   }
   else
   {
      // only Write Multiple Ext uses multi_cnt
      if ( cmd != ATACMD_WRITE_MULTIPLE_EXT )
         multi_cnt = 1;
   }

   channel->reg_cmd_info.ns  = num_sect;
   channel->reg_cmd_info.mc  = multi_cnt;

   return exec_pio_data_out_cmd( channel, dev, address, num_sect, multi_cnt );
}

//*************************************************************
//
// reg_packet() - Execute an ATAPI Packet (A0H) command.
//
// See ATA-4 Section 9.10, Figure 14.
//
//*************************************************************

int reg_packet( struct ata_channel *channel, int dev,
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
   unsigned int byteCnt;
   long word_cnt;
   int ndx;
   unsigned long dpaddr;
   unsigned long savedpaddr;
   unsigned char *cfp;
   int slowXferCntr = 0;
   int prevFailBit7 = 0;

   // Make sure the command packet size is either 12 or 16
   // and save the command packet size and data.
	channel->int_intr_flag = 0;

   cpbc = cpbc < 12 ? 12 : cpbc;
   cpbc = cpbc > 12 ? 16 : cpbc;

   // Setup current command information.
   sub_zero_return_data(channel);
   channel->reg_cmd_info.flg = TRC_FLAG_ATAPI;
   channel->reg_cmd_info.ct  = dir ? TRC_TYPE_PPDO : TRC_TYPE_PPDI;
   channel->reg_cmd_info.cmd = ATACMD_PACKET;
   channel->reg_cmd_info.fr1 = channel->reg_atapi_reg_fr;
   channel->reg_cmd_info.sc1 = channel->reg_atapi_reg_sc;
   channel->reg_cmd_info.sn1 = channel->reg_atapi_reg_sn;
   channel->reg_cmd_info.cl1 = (unsigned char) ( dpbc & 0x00ff );
   channel->reg_cmd_info.ch1 = ( unsigned char) ( ( dpbc & 0xff00 ) >> 8 );
   channel->reg_cmd_info.dh1 = ( dev ? CB_DH_DEV1 : CB_DH_DEV0 )
                      | ( channel->reg_atapi_reg_dh & 0x0f );
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

   // Set command time out.
   SIGNALHANDLER sigh = tmr_set_timeout(channel);

   // Select the drive - call the sub_select function.
   // Quit now if this fails.

   if ( sub_select( channel, dev ) )
   {
      sub_trace_command(channel);
      channel->reg_drq_block_call_back = (void *) 0;
	  tmr_discard_timeout(sigh);
      return 1;
   }

   // Set up all the registers except the command register.

   sub_setup_command(channel);

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
	if ( ( status & CB_STAT_BSY ) != 0 )               // time out ?
	{
		channel->reg_cmd_info.to = 1;
		channel->reg_cmd_info.ec = 51;
		dir = -1;   // command done
	}

   // Command packet transfer...
   // Check for protocol failures... no interrupt here please!
   // Clear any interrupt the command packet transfer may have caused.

   if ( channel->int_intr_flag )       // extra interrupt(s) ?
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

      if (    ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) ) != CB_STAT_DRQ)
      {
         channel->reg_cmd_info.ec = 52;
         dir = -1;   // command done
      }
      else
      {
         // Command packet transfer...
         // Check for protocol failures...
         // check: C/nD=1, IO=0.

         if ( ( reason &  ( CB_SC_P_TAG | CB_SC_P_REL | CB_SC_P_IO ) ) || ( ! ( reason &  CB_SC_P_CD ) ) )
            channel->reg_cmd_info.failbits |= FAILBIT2;
         
		 if ( ( lowCyl != channel->reg_cmd_info.cl1 ) || ( highCyl != channel->reg_cmd_info.ch1 ) )
            channel->reg_cmd_info.failbits |= FAILBIT3;

         // Command packet transfer...
         // trace cdb byte 0,
         // xfer the command packet (the cdb)

         pio_drq_block_out( channel, CB_DATA, cpaddress, cpbc >> 1 );

         DELAY400NS;    // delay so device can get the status updated
      }
   }

   // Data transfer loop...
   // If there is no error, enter the data transfer loop.
   // First adjust the I/O buffer address so we are able to
   // transfer large amounts of data (more than 64K).

   dpaddr = dpaddress;
   savedpaddr = dpaddr;

   while ( channel->reg_cmd_info.ec == 0 )
   {
      // Data transfer loop...
      // Wait for interrupt -or- wait for not BUSY -or- wait for time out.
      sub_atapi_delay( channel, dev );
      reg_wait_poll( channel, 53, 54 );

      // Data transfer loop...
      // If there was a time out error, exit the data transfer loop.

      if ( channel->reg_cmd_info.ec )
      {
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // If using interrupts get the status read by the interrupt
      // handler, otherwise read the status register.

      if ( channel->int_use_intr_flag )
         status = channel->int_ata_status;
      else
         status = pio_inbyte( channel, CB_STAT );
      reason = pio_inbyte( channel, CB_SC );
      lowCyl = pio_inbyte( channel, CB_CL );
      highCyl = pio_inbyte( channel, CB_CH );

      // Data transfer loop...
      // Exit the read data loop if the device indicates this
      // is the end of the command.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
      {
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // The device must want to transfer data...
      // check status: must have BSY=0, DRQ=1 now.

      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) != CB_STAT_DRQ )
      {
         channel->reg_cmd_info.ec = 55;
         dir = -1;   // command done
         break;
      }
      else
      {
         // Data transfer loop...
         // Check for protocol failures...
         // check: C/nD=0, IO=1 (read) or IO=0 (write).

         if ( ( reason &  ( CB_SC_P_TAG | CB_SC_P_REL ) )
              || ( reason &  CB_SC_P_CD )
            )
            channel->reg_cmd_info.failbits |= FAILBIT4;
         if ( ( reason & CB_SC_P_IO ) && dir )
            channel->reg_cmd_info.failbits |= FAILBIT5;
      }

      // do the slow data transfer thing

      if ( channel->reg_slow_xfer_flag )
      {
         slowXferCntr ++ ;
         if ( slowXferCntr <= channel->reg_slow_xfer_flag )
         {
            sub_xfer_delay(channel);
            channel->reg_slow_xfer_flag = 0;
         }
      }

      // Data transfer loop...
      // get the byte count, check for zero...

      byteCnt = ( highCyl << 8 ) | lowCyl;
      if ( byteCnt < 1 )
      {
         channel->reg_cmd_info.ec = 59;
         dir = -1;   // command done
         break;
      }

      // Data transfer loop...
      // and check protocol failures...

      if ( byteCnt > dpbc )
         channel->reg_cmd_info.failbits |= FAILBIT6;

      channel->reg_cmd_info.failbits |= prevFailBit7;
      
	  prevFailBit7 = 0;
      if ( byteCnt & 0x0001 )
         prevFailBit7 = FAILBIT7;

      // Data transfer loop...
      // Quit if buffer overrun.
      // If DRQ call back in use adjust buffer address.

      if ( channel->reg_drq_block_call_back )
      {
         if ( byteCnt > channel->reg_buffer_size )
         {
            channel->reg_cmd_info.ec = 61;
            dir = -1;   // command done
            break;
         }
         channel->reg_cmd_info.drqPacketSize = byteCnt;
         dpaddr = savedpaddr;
      }
      else
      {
         if ( ( channel->reg_cmd_info.totalBytesXfer + byteCnt ) > channel->reg_buffer_size )
         {
            channel->reg_cmd_info.ec = 61;
            dir = -1;   // command done
            break;
         }
      }

      // increment number of DRQ packets
      channel->reg_cmd_info.drqPackets ++ ;

      // Data transfer loop...
      // transfer the data and update the i/o buffer address
      // and the number of bytes transfered.

      word_cnt = ( byteCnt >> 1 ) + ( byteCnt & 0x0001 );
      channel->reg_cmd_info.totalBytesXfer += ( word_cnt << 1 );
      dpaddress = dpaddr;
      if ( dir )
      {
         if ( channel->reg_drq_block_call_back )
            (* channel->reg_drq_block_call_back) ( channel, &channel->reg_cmd_info );
         pio_drq_block_out( channel, CB_DATA, dpaddress, word_cnt );
      }
      else
      {
         pio_drq_block_in( channel, CB_DATA, dpaddress, word_cnt );
         if ( channel->reg_drq_block_call_back )
            (* channel->reg_drq_block_call_back) ( channel, &channel->reg_cmd_info );
      }
      dpaddr = dpaddr + byteCnt;

      DELAY400NS;    // delay so device can get the status updated
   }

   // End of command...
   // Wait for interrupt or poll for BSY=0,
   // but don't do this if there was any error or if this
   // was a commmand that did not transfer data.

   if ( ( channel->reg_cmd_info.ec == 0 ) && ( dir >= 0 ) )
   {
      sub_atapi_delay( channel, dev );
      reg_wait_poll( channel, 56, 57 );
   }

   // Final status check, only if no previous error.

   if ( channel->reg_cmd_info.ec == 0 )
   {
      // Final status check...
      // If using interrupts get the status read by the interrupt
      // handler, otherwise read the status register.

      if ( channel->int_use_intr_flag )
         status = channel->int_ata_status;
      else
         status = pio_inbyte( channel, CB_STAT );
      reason = pio_inbyte( channel, CB_SC );
      lowCyl = pio_inbyte( channel, CB_CL );
      highCyl = pio_inbyte( channel, CB_CH );

      // Final status check...
      // check for any error.

      if ( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )
      {
         channel->reg_cmd_info.ec = 58;
      }
   }

   // Final status check...
   // Check for protocol failures...
   // check: C/nD=1, IO=1.

   if ( ( reason & ( CB_SC_P_TAG | CB_SC_P_REL ) )
        || ( ! ( reason & CB_SC_P_IO ) )
        || ( ! ( reason & CB_SC_P_CD ) )
      )
      channel->reg_cmd_info.failbits |= FAILBIT8;

   // Done...
   // Read the output registers and trace the command.
   if ( ! channel->reg_cmd_info.totalBytesXfer )
      channel->reg_cmd_info.ct = TRC_TYPE_PND;
   sub_trace_command(channel);

	tmr_discard_timeout(sigh);

   // reset reg_drq_block_call_back to NULL (0)

   channel->reg_drq_block_call_back = (void *) 0;

   // All done.  The return values of this function are described in
   // ATAIO.H.

   if ( channel->reg_cmd_info.ec )
      return 1;
   return 0;
}

// end ataioreg.c
