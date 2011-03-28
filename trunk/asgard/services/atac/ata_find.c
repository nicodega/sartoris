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
#include <services/pmanager/services.h>

#define CHANNEL_BUFFER_SIZE 0x20000		// 128kb per channel
char channel_buffers[2][CHANNEL_BUFFER_SIZE];

int ata_adapters_count;
struct ata_host_adapter ata_adapters[MAX_ADAPTERS];	// adapters on the system

/* Find ata devices on the system.
In a future this should use the pci service in order to find out about
bus mastering options, etc. By now, ataiopci is not supported (hence 
ultra dma is not available).
*/
void ata_find()
{
	int i = 0;
	
	/* This implementation is VERY rudimentary, it should 
	be changed to support many more features.
	For now it'll assume there is only one ata host adapter, 
	and it will find devices on both of its channels using IDE 
	legacy ports. */
	
	ata_adapters_count = 1;
	ata_adapters[0].active = 1;

	/* Now setup channels for PIO without interrupts 
	and issue a reg_config. */

	for(i = 0; i < CHANNEL_BUFFER_SIZE; i++)
	{
		channel_buffers[0][i] = -1;
		channel_buffers[1][i] = -1;
	}

	/* Setup channel 1 */
	setup_channel(&ata_adapters[0].channels[0], 0, 16, 0x1F0, 0x3F0, channel_buffers[0], CHANNEL_BUFFER_SIZE, 0);

	ata_adapters[0].channels[0].irq = 14;

	/* Setup channel 2 */
	setup_channel(&ata_adapters[0].channels[1], 1, 16, 0x170, 0x370, channel_buffers[1], CHANNEL_BUFFER_SIZE, 0);

	ata_adapters[0].channels[1].irq = 15;
	
	/* config channel 1 */
	reg_config(&ata_adapters[0].channels[0]);

	/* config channel 2 */
	reg_config(&ata_adapters[0].channels[1]);

	// do an ATA soft reset on each present device (SRST)
	if(ata_adapters[0].channels[0].devices[0].reg_config_info != REG_CONFIG_TYPE_NONE && ata_adapters[0].channels[0].devices[0].reg_config_info != REG_CONFIG_TYPE_UNKN)
	{
		reg_reset( &ata_adapters[0].channels[0], 0, 0 );
		// setup device information 
		if(identify_device(&ata_adapters[0].channels[0], 0))
		{
			ata_adapters[0].channels[0].devices[0].reg_config_info = REG_CONFIG_TYPE_NONE;
			return ;
		}
		ldev_find(&ata_adapters[0].channels[0], 0);
		ata_adapters[0].channels[0].devices_count++;
	}
	if(ata_adapters[0].channels[0].devices[1].reg_config_info != REG_CONFIG_TYPE_NONE && ata_adapters[0].channels[0].devices[1].reg_config_info != REG_CONFIG_TYPE_UNKN)
	{		
		reg_reset( &ata_adapters[0].channels[0], 0, 1 );
		// setup device information 
		if(identify_device(&ata_adapters[0].channels[0], 1))
		{
			ata_adapters[0].channels[0].devices[1].reg_config_info = REG_CONFIG_TYPE_NONE;
			return ;
		}
		ldev_find(&ata_adapters[0].channels[0], 0);
		ata_adapters[0].channels[0].devices_count++;
	}
	if(ata_adapters[0].channels[1].devices[0].reg_config_info != REG_CONFIG_TYPE_NONE && ata_adapters[0].channels[1].devices[0].reg_config_info != REG_CONFIG_TYPE_UNKN)
	{
		reg_reset( &ata_adapters[0].channels[1], 0, 0 );
		// setup device information 
		if(identify_device(&ata_adapters[0].channels[1], 0))
		{
			ata_adapters[0].channels[1].devices[0].reg_config_info = REG_CONFIG_TYPE_NONE;
			return ;
		}
		ldev_find(&ata_adapters[0].channels[1], 0);
		ata_adapters[0].channels[1].devices_count++;
	}
	if(ata_adapters[0].channels[1].devices[1].reg_config_info != REG_CONFIG_TYPE_NONE && ata_adapters[0].channels[1].devices[1].reg_config_info != REG_CONFIG_TYPE_UNKN)
	{
		reg_reset( &ata_adapters[0].channels[1], 0, 1 );
		// setup device information 
		if(identify_device(&ata_adapters[0].channels[1], 1))
		{
			ata_adapters[0].channels[1].devices[1].reg_config_info = REG_CONFIG_TYPE_NONE;
			return ;
		}
		ldev_find(&ata_adapters[0].channels[1], 0);
		ata_adapters[0].channels[1].devices_count++;
	}

	if(ata_adapters[0].channels[0].devices_count > 0)
		create_channel_wp(&ata_adapters[0].channels[0]);

	if(ata_adapters[0].channels[1].devices_count > 0)
		create_channel_wp(&ata_adapters[0].channels[1]);
}

void create_channel_wp(struct ata_channel *channel)
{
	/* Tell the process manager to create a thread for us */
	struct pm_msg_create_thread msg_create_thr;
	struct pm_msg_response      msg_res;
	int sender_id;

	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = 0;
	msg_create_thr.response_port = ATAC_THREAD_ACK_PORT;
	msg_create_thr.task_id = get_current_task();
	msg_create_thr.flags = 0;
	msg_create_thr.interrupt = 0;
	msg_create_thr.entry_point = &channel_wp;

	channelwp_channel = channel;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	while (get_msg_count(ATAC_THREAD_ACK_PORT) == 0) reschedule();

	get_msg(ATAC_THREAD_ACK_PORT, &msg_res, &sender_id);

	if (msg_res.status != PM_THREAD_OK) 
	{
		__asm__ ("cli"::);
		print(":(",0);
        __asm__ __volatile__ ("xchg %%bx, %%bx "::);
		for(;;);
	}

	channel->thread = msg_res.new_id;

	while(!channel->initialized){ reschedule(); }
}

int identify_device(struct ata_channel *channel, int devnum)
{
	struct ata_device *dev = &channel->devices[devnum];
#ifdef ATAC_LBAOP
	// do an ATA Identify command in LBA28 mode
	if(reg_pio_data_in_lba28(&ata_adapters[0].channels[0],
               devnum, ATACMD_IDENTIFY_DEVICE,
               0, 0,
               0,
               (unsigned int)channel->reg_buffer,
               1, 0 ))
	{
		__asm__ ("cli"::);
		print("identify drive failed");
		for(;;);
	}
#else
	if(reg_pio_data_in_chs(channel, devnum, ATACMD_IDENTIFY_DEVICE,
                         0, 0,
                         0, 0, 0,
                         (unsigned int)channel->reg_buffer,
                         1, 0 ))
	{
		__asm__ ("cli"::);
		print("identify drive failed");
		for(;;);
	}
#endif
	// read data from identify drive 
	dev->heads = *(((unsigned short*)channel->reg_buffer) + 3);
	dev->cylinders = *(((unsigned short*)channel->reg_buffer) + 1);
	dev->sect_per_track = *(((unsigned short*)channel->reg_buffer) + 6);
	dev->sector_count = *((unsigned int*)(((unsigned short*)channel->reg_buffer) + 60));	// hoy many sectors do we have

	if(dev->sector_count == 0)
	{
		dev->sector_count = dev->heads * dev->sect_per_track * dev->cylinders;
	}
        
	int i = 0;
	char *ser = (char * )(((unsigned short*)channel->reg_buffer) + 10);
	while(i < 20)
	{
		dev->serial[i] = ser[i];
		i++;
	}
	dev->serial[20] = '\0';

	unsigned short dma = *(((unsigned short*)channel->reg_buffer) + 63);

	dev->supported_multiword = dma & 0x3;
	
	dma = (dma >> 8) & 0x7;

	if(dma & 0x1)
	{
		dev->supported_multiword_selected = DMA_MULTIWORD0_SELECTED;
	}
	else if(dma & 0x2)
	{
		dev->supported_multiword_selected = DMA_MULTIWORD1_SELECTED;
	}
	else if(dma & 0x4)
	{
		dev->supported_multiword_selected = DMA_MULTIWORD2_SELECTED;
	}

	dev->rwmultiple_max = ((*(((unsigned short*)channel->reg_buffer) + 47)) & 0xFF);	

	if(dev->rwmultiple_max == 0)
	{
		dev->rwmultiple_max = 4;	
	}
	
	dev->ata_version = *(((unsigned short*)channel->reg_buffer) + 80);

	if(!(dev->ata_version & SUPPORTED_ATA_VER))
	{
		return 1;
	}

	// select addressing mode
#ifndef ATAC_LBAOP
	if(dev->sector_count <= 16514064)
	{
		dev->address_mode = ATAC_ADDRESMODE_CHS;
	}
	else 
#endif
	if(dev->sector_count <= (unsigned int)(unsigned long)0xFFFFFFF)
	{
		dev->address_mode = ATAC_ADDRESMODE_LBA28;
	}
	else
	{
		dev->address_mode = ATAC_ADDRESMODE_LBA48;
	}
	return 0;	// ok
}

/* Setup initial channel configuration (PIO, no ints, no DMA) */
void setup_channel(struct ata_channel *channel, unsigned int id, int pio_xfer_width,
				   unsigned int base_command_reg, unsigned int base_control_reg,
				   char *buffer, unsigned int buffer_size, int data_adapterid)
{
	channel->id = id;
	channel->pio_xfer_width = pio_xfer_width;

	/* Setup base ports and offsets */
	pio_set_iobase_addr(channel, base_command_reg, base_control_reg);

	channel->reg_slow_xfer_flag = 0;

	channel->reg_buffer_size = buffer_size;
	channel->reg_buffer = (unsigned char *)buffer;

	channel->int_use_intr_flag = 0;		// initially no interrupts
	channel->irq = 0;
	channel->int_bm_status = 0;
	channel->int_ata_status = base_control_reg + 7;

	channel->int_stack = NULL;
	channel->dma_mode = DMA_MODE_NONE;	// initially no dma.

	queue_init(&channel->queue);

	channel->thread = -1;
	channel->initialized = 0;
	channel->dma_man_smo = -1;
	channel->dma_count = 0;

	channel->devices[0].mode = ATAC_DEVMODE_PIO;
	channel->devices[1].mode = ATAC_DEVMODE_PIO;
	channel->int_thread_created = 0;
	channel->devices_count = 0;

	channel->int_ata_addr = base_command_reg + 7;
	channel->reg_drq_block_call_back = 0;

	channel->pio_memory_dt_opt = PIO_MEMORY_DT_OPT0;
	channel->int_thread_created = 0;
	channel->initialized = 0;
	channel->data_adapterid = data_adapterid;
}


