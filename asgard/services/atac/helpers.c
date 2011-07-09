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
*
*/

#include "atac.h"

/* This function will set channel parameters, before command 
execution for a device.
*/
int channel_configure(struct ata_channel *channel, int dev)
{
	/* If device interrupts are enabled, set the flag */
	if((channel->devices[dev].mode & ATAC_DEVMODE_INT)
        && channel->int_use_intr_flag)	
	{
		print("ATAC: creating int handler for channel %i\n", channel->id);
		
		channel->int_intr_flag = 1;

		/* If interrupt handler has not been created yet, create it */
		if(!channel->int_thread_created)
		{
			if(int_enable_irq(channel))
			{
				channel->int_intr_flag = 0;	// hmm thread creation failed.. fall back.
			}
		}
	}
	else
	{
		channel->int_intr_flag = 0;	// disable channel interrupts
	}
}

void convert_from_LBA(struct ata_device *dev, int block, int *head, int *track, int *sector) 
{

	*track = block / (dev->sect_per_track * dev->heads);
	*head = (block / dev->sect_per_track) % dev->heads;
	*sector = (block % dev->sect_per_track) + 1;
}

/* Execute a read command. */
int channel_read(struct ata_channel *channel, int dev, unsigned int address, unsigned long long lba, unsigned int count)
{
	int head, sector, track;

	if(channel->devices[dev].reg_config_info == REG_CONFIG_TYPE_ATA)
	{
		channel_configure(channel, dev);

		/* Select reading function by using the device address_mode */
		switch(channel->devices[dev].address_mode)
		{
			case ATAC_ADDRESMODE_CHS:
				convert_from_LBA(&channel->devices[dev], lba, &head, &track, &sector);
				return reg_pio_data_in_chs( channel, dev, ATACMD_READ_SECTORS, 0, count, track, head, sector, address, count, 0);
				break;
			case ATAC_ADDRESMODE_LBA28:
				return reg_pio_data_in_lba28(channel, dev, ATACMD_READ_SECTORS, 0, count, lba, address, count, 0);
				break;
			case ATAC_ADDRESMODE_LBA32:
			case ATAC_ADDRESMODE_LBA48:
				return reg_pio_data_in_lba48(channel, dev, ATACMD_READ_SECTORS, 0, count, (unsigned long)(lba >> 32), (unsigned long)(lba & 0xFFFFFFFF), address, count, 0);
				break;
		}
	}
}

/* Execute a write command.
data will be read from the channel buffer. */
int channel_write(struct ata_channel *channel, int dev, unsigned int address, unsigned long long lba, unsigned int count)
{
	int head, sector, track;

	if(channel->devices[dev].reg_config_info == REG_CONFIG_TYPE_ATA)
	{
		channel_configure(channel, dev);
		/* Select reading function by using the device address_mode */
		switch(channel->devices[dev].address_mode)
		{
			case ATAC_ADDRESMODE_CHS:
				convert_from_LBA(&channel->devices[dev], lba, &head, &track, &sector);
				return reg_pio_data_out_chs( channel, dev, ATACMD_WRITE_SECTORS, 0, count, track, head, sector, address, count, 0);
				break;
			case ATAC_ADDRESMODE_LBA28:
				return reg_pio_data_out_lba28(channel, dev, ATACMD_WRITE_SECTORS, 0, count, lba, address, count, 0);
				break;
			case ATAC_ADDRESMODE_LBA32:
			case ATAC_ADDRESMODE_LBA48:
				return reg_pio_data_out_lba48(channel, dev, ATACMD_WRITE_SECTORS, 0, count, (unsigned long)(lba >> 32), (unsigned long)(lba & 0xFFFFFFFF), address, count, 0);
				break;
		}
	}
}


