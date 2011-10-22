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
#include <lib/signals.h>

//*************************************************************
//
// sub_zero_return_data() -- zero the return data areas.
//
//*************************************************************

void sub_zero_return_data( struct ata_channel *channel )
{
   int ndx;

   for ( ndx = 0; ndx < sizeof( struct REG_CMD_INFO ); ndx ++ )
      ( (unsigned char *) &channel->reg_cmd_info )[ndx] = 0;
}

//*************************************************************
//
// sub_setup_command() -- setup the command parameters
//                        in FR, SC, SN, CL, CH and DH.
//
//*************************************************************

void sub_setup_command( struct ata_channel *channel )

{
   unsigned char fr48[2];
   unsigned char sc48[2];
   unsigned char lba48[8];

   * (unsigned short *) fr48 = channel->reg_cmd_info.fr1;
   * (unsigned short *) sc48 = channel->reg_cmd_info.sc1;
   * (unsigned int *) ( lba48 + 4 ) = channel->reg_cmd_info.lbaHigh1;
   * (unsigned int *) ( lba48 + 0 ) = channel->reg_cmd_info.lbaLow1;

   channel->int_ata_status = CB_STAT_BSY;

   pio_outbyte( channel, CB_DC, channel->reg_cmd_info.dc1 );

   if ( channel->reg_cmd_info.lbaSize == LBA28 )
   {
      // in ATA LBA28 mode
      channel->reg_cmd_info.fr1 = fr48[0];
      pio_outbyte( channel, CB_FR, fr48[0] );
      channel->reg_cmd_info.sc1 = sc48[0];
      pio_outbyte( channel, CB_SC, sc48[0] );
      channel->reg_cmd_info.sn1 = lba48[0];
      pio_outbyte( channel, CB_SN, lba48[0] );
      channel->reg_cmd_info.cl1 = lba48[1];
      pio_outbyte( channel, CB_CL, lba48[1] );
      channel->reg_cmd_info.ch1 = lba48[2];
      pio_outbyte( channel, CB_CH, lba48[2] );
      channel->reg_cmd_info.dh1 = ( channel->reg_cmd_info.dh1 & 0xf0 ) | ( lba48[3] & 0x0f );
      pio_outbyte( channel, CB_DH, channel->reg_cmd_info.dh1 );
   }
   else
   if ( channel->reg_cmd_info.lbaSize == LBA48 )
   {
      // in ATA LBA48 mode
      pio_outbyte( channel, CB_FR, fr48[1] );
      pio_outbyte( channel, CB_SC, sc48[1] );
      pio_outbyte( channel, CB_SN, lba48[3] );
      pio_outbyte( channel, CB_CL, lba48[4] );
      pio_outbyte( channel, CB_CH, lba48[5] );
      channel->reg_cmd_info.fr1 = fr48[0];
      pio_outbyte( channel, CB_FR, fr48[0] );
      channel->reg_cmd_info.sc1 = sc48[0];
      pio_outbyte( channel, CB_SC, sc48[0] );
      channel->reg_cmd_info.sn1 = lba48[0];
      pio_outbyte( channel, CB_SN, lba48[0] );
	  channel->reg_cmd_info.cl1 = lba48[1];
      pio_outbyte( channel, CB_CL, lba48[1] );
      channel->reg_cmd_info.ch1 = lba48[2];
      pio_outbyte( channel, CB_CH, lba48[2] );
      pio_outbyte( channel, CB_DH, channel->reg_cmd_info.dh1  );
   }
   else
   {
      // in ATA CHS or ATAPI LBA32 mode
      pio_outbyte( channel, CB_FR, channel->reg_cmd_info.fr1  );
      pio_outbyte( channel, CB_SC, channel->reg_cmd_info.sc1  );
      pio_outbyte( channel, CB_SN, channel->reg_cmd_info.sn1  );
      pio_outbyte( channel, CB_CL, channel->reg_cmd_info.cl1  );
      pio_outbyte( channel, CB_CH, channel->reg_cmd_info.ch1  );
      pio_outbyte( channel, CB_DH, channel->reg_cmd_info.dh1  );
   }
}

//*************************************************************
//
// sub_trace_command() -- trace the end of a command.
//
//*************************************************************

void sub_trace_command( struct ata_channel *channel )

{
   unsigned long lba;
   unsigned char sc48[2];
   unsigned char lba48[8];

   channel->reg_cmd_info.st2 = pio_inbyte( channel, CB_STAT );
   channel->reg_cmd_info.as2 = pio_inbyte( channel, CB_ASTAT );
   channel->reg_cmd_info.er2 = pio_inbyte( channel, CB_ERR );
   if ( channel->reg_cmd_info.lbaSize == LBA48 )
   {
      // read back ATA LBA48...
      sc48[0]  = pio_inbyte( channel,CB_SC );
      lba48[0] = pio_inbyte( channel,CB_SN );
      lba48[1] = pio_inbyte( channel,CB_CL );
      lba48[2] = pio_inbyte( channel,CB_CH );
      pio_outbyte( channel,CB_DC, CB_DC_HOB );
      sc48[1]  = pio_inbyte( channel,CB_SC );
      lba48[3] = pio_inbyte( channel,CB_SN );
      channel->reg_cmd_info.sn2 = lba48[3];
      lba48[4] = pio_inbyte( channel,CB_CL );
      channel->reg_cmd_info.cl2 = lba48[4];
      lba48[5] = pio_inbyte( channel,CB_CH );
      channel->reg_cmd_info.ch2 = lba48[5];
      lba48[6] = 0;
      lba48[7] = 0;
      channel->reg_cmd_info.sc2 = * (unsigned short *) sc48;
      channel->reg_cmd_info.lbaHigh2 = * (unsigned long *) ( lba48 + 4 );
      channel->reg_cmd_info.lbaLow2  = * (unsigned long *) ( lba48 + 0 );
      channel->reg_cmd_info.dh2 = pio_inbyte( channel,CB_DH );
   }
   else
   {
      // read back ATA CHS, ATA LBA28 or ATAPI LBA32
      channel->reg_cmd_info.sc2 = pio_inbyte( channel,CB_SC );
      channel->reg_cmd_info.sn2 = pio_inbyte( channel,CB_SN );
      channel->reg_cmd_info.cl2 = pio_inbyte( channel,CB_CL );
      channel->reg_cmd_info.ch2 = pio_inbyte( channel,CB_CH );
      channel->reg_cmd_info.dh2 = pio_inbyte( channel,CB_DH );
      channel->reg_cmd_info.lbaHigh2 = 0;
      channel->reg_cmd_info.lbaLow2 = 0;
      if ( channel->reg_cmd_info.lbaSize == LBA28 )
      {
         lba = channel->reg_cmd_info.dh2 & 0x0f;
         lba = lba << 8;
         lba = lba | channel->reg_cmd_info.ch2;
         lba = lba << 8;
         lba = lba | channel->reg_cmd_info.cl2;
         lba = lba << 8;
         lba = lba | channel->reg_cmd_info.sn2;
         channel->reg_cmd_info.lbaLow2 = lba;
      }
   }
}

//*************************************************************
//
// sub_select() - function used to select a drive.
//
// Function to select a drive. This subroutine waits for not BUSY,
// selects a drive and waits for READY and SEEK COMPLETE status.
//
//**************************************************************

int sub_select( struct ata_channel *channel, int dev )

{
   unsigned char status;

   // PAY ATTENTION HERE
   // The caller may want to issue a command to a device that doesn't
   // exist (for example, Exec Dev Diag), so if we see this,
   // just select that device, skip all status checking and return.
   // We assume the caller knows what they are doing!

   if ( channel->devices[dev].reg_config_info < REG_CONFIG_TYPE_ATA )
   {
      // select the device and return

      pio_outbyte( channel, CB_DH, dev ? CB_DH_DEV1 : CB_DH_DEV0 );
      DELAY400NS;
      return 0;
   }

   // The rest of this is the normal ATA stuff for device selection
   // and we don't expect the caller to be selecting a device that
   // does not exist.
   // We don't know which drive is currently selected but we should
   // wait for it to be not BUSY.  Normally it will be not BUSY
   // unless something is very wrong!

   SIGNALHANDLER sigh = tmr_set_timeout(channel);

   while ( tmr_check_timeout(sigh) == 0 )
   {
      status = pio_inbyte( channel, CB_STAT );
      if ( ( status & CB_STAT_BSY ) == 0 )
         break;
   }

   if ( ( status & CB_STAT_BSY ) != 0 )
	{
		channel->reg_cmd_info.to = 1;
		channel->reg_cmd_info.ec = 11;
		
		channel->reg_cmd_info.st2 = status;
		channel->reg_cmd_info.as2 = pio_inbyte( channel, CB_ASTAT );
		channel->reg_cmd_info.er2 = pio_inbyte( channel, CB_ERR );
		channel->reg_cmd_info.sc2 = pio_inbyte( channel, CB_SC );
		channel->reg_cmd_info.sn2 = pio_inbyte( channel, CB_SN );
		channel->reg_cmd_info.cl2 = pio_inbyte( channel, CB_CL );
		channel->reg_cmd_info.ch2 = pio_inbyte( channel, CB_CH );
		channel->reg_cmd_info.dh2 = pio_inbyte( channel, CB_DH );
		return 1;
	}

	tmr_discard_timeout(sigh);

   // Here we select the drive we really want to work with by
   // putting 0xA0 or 0xB0 in the Drive/Head register (1f6).

   pio_outbyte( channel, CB_DH, dev ? CB_DH_DEV1 : CB_DH_DEV0 );
   DELAY400NS;

   // If the selected device is an ATA device,
   // wait for it to have READY and SEEK COMPLETE
   // status.  Normally the drive should be in this state unless
   // something is very wrong (or initial power up is still in
   // progress).  For any other type of device, just wait for
   // BSY=0 and assume the caller knows what they are doing.

   sigh = tmr_set_timeout(channel);
  
   int ok = 0;
   while ( tmr_check_timeout(sigh)  == 0 )
   {
      status = pio_inbyte( channel, CB_STAT );
      if ( channel->devices[dev].reg_config_info == REG_CONFIG_TYPE_ATA )
      {
			if ( ( status & ( CB_STAT_BSY | CB_STAT_RDY | CB_STAT_SKC ) )
					== ( CB_STAT_RDY | CB_STAT_SKC ) )
			{
				ok = 1;
				break;
			}
      }
      else
      {
			if ( ( status & CB_STAT_BSY ) == 0 )
			{
				ok = 1;
				break;
			}
      }
   }

   if ( !ok )
    {
        channel->reg_cmd_info.to = 1;
        channel->reg_cmd_info.ec = 12;
       
		channel->reg_cmd_info.st2 = status;
        channel->reg_cmd_info.as2 = pio_inbyte( channel,CB_ASTAT );
        channel->reg_cmd_info.er2 = pio_inbyte( channel,CB_ERR );
        channel->reg_cmd_info.sc2 = pio_inbyte( channel,CB_SC );
        channel->reg_cmd_info.sn2 = pio_inbyte( channel,CB_SN );
        channel->reg_cmd_info.cl2 = pio_inbyte( channel,CB_CL );
        channel->reg_cmd_info.ch2 = pio_inbyte( channel,CB_CH );
        channel->reg_cmd_info.dh2 = pio_inbyte( channel,CB_DH );
        return 1;
    }

	tmr_discard_timeout(sigh);

   // All done.  The return values of this function are described in
   // ATAIO.H.

   if ( channel->reg_cmd_info.ec )
      return 1;
   return 0;
}

//*************************************************************
//
// sub_atapi_delay() - delay for at least two ticks of the bios
//                     timer (or at least 110ms).
//
//*************************************************************

void sub_atapi_delay( struct ata_channel *channel, int dev )

{
   int ndx;
   long lw;

   if ( channel->devices[dev].reg_config_info != REG_CONFIG_TYPE_ATAPI )
      return;

   if ( ! channel->reg_atapi_delay_flag )
      return;

   /* sleep for 110ms */
   sleep(110);
}

//*************************************************************
//
// sub_xfer_delay() - delay until the bios timer ticks
//                    (from 0 to 55ms).
//
//*************************************************************

void sub_xfer_delay( struct ata_channel *channel )

{
    long lw;

    sleep(55);
}

