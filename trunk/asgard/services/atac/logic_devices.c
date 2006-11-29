/*
*
*	Functions defined on this file, allows the driver to see a a single disk as a set of
*	logic devices (for disks this would read partitions and export them as multiple disks)
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
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

int ldev_valdid(int id)
{
	if(id & STDDEV_PHDEV_FLAG)
	{
		id = id & !STDDEV_PHDEV_FLAG;

		// it's a physical drive
		// check if it's present
		if(id > (ata_adapters_count*4)) return 0;

		int dev = id % 2;
		int channel = (id >> 1);

		if((ata_adapters[channel / 2].channels[channel % 2].devices[dev].reg_config_info == REG_CONFIG_TYPE_NONE 
			|| ata_adapters[channel / 2].channels[channel % 2].devices[dev].reg_config_info == REG_CONFIG_TYPE_UNKN)) 
			return 0;
		
		return 1;
	}	
	else
	{
		return (ldev_get(id) != NULL);
	}
}

struct logic_device *ldev_get(int id)
{
	if(id & STDDEV_PHDEV_FLAG)
	{
		id = id & !STDDEV_PHDEV_FLAG;

		// it's a physical drive
		// check if it's present
		if(id >= (ata_adapters_count*4)) return NULL;

		int dev = id % 2;
		int channel = (id >> 1);

		if((ata_adapters[channel / 2].channels[channel % 2].devices[dev].reg_config_info == REG_CONFIG_TYPE_NONE 
			|| ata_adapters[channel / 2].channels[channel % 2].devices[dev].reg_config_info == REG_CONFIG_TYPE_UNKN)) 
			return NULL;
		
		return &ata_adapters[channel / 2].channels[channel % 2].devices[dev].ldev;
	}	
	else
	{
		/* It's a logic device */
		CPOSITION it;
		struct logic_device *ldev = NULL;

		/* Get channel, adapter and device id */
		int ldevid = id & 0xFF;
		int dev = ((id >> 8) & 0x1);
		int channel = (id >> 9);
		int adapter = (channel >> 1);

		if(adapter >= ata_adapters_count || channel >= ata_adapters_count*4
			|| ldevid >= length(&ata_adapters[adapter].channels[channel & 0x1].devices[dev].logic_devs))
			return NULL;

		if((ata_adapters[channel / 2].channels[channel & 0x1].devices[dev].reg_config_info == REG_CONFIG_TYPE_NONE 
			|| ata_adapters[channel / 2].channels[channel & 0x1].devices[dev].reg_config_info == REG_CONFIG_TYPE_UNKN)) 
			return NULL;

		/* Find ldev on the list and return it */
		it = get_head_position(&ata_adapters[adapter].channels[channel & 0x1].devices[dev].logic_devs);

		while(it != NULL)
		{
			ldev = (struct logic_device *)get_next(&it);
			if(ldev->id == ldevid) return ldev;
		}
		return NULL;	
	}
}

int ldev_stdget(int id, int task, int exclusive)
{
	struct logic_device *ld, *ldev;
	CPOSITION it = NULL;
	int pdevid, ldevid;

	ldev = ldev_get(id);

	if(ldev == NULL) return 1;

	int dev, channel, adapter;

	if(id & STDDEV_PHDEV_FLAG)
	{
		pdevid = id & ~STDDEV_PHDEV_FLAG;

		dev = id % 2;
		channel = (id >> 1);
		adapter = (channel >> 1);
		ldevid = -1;
	}
	else
	{
		ldevid = id & 0xFF;
		dev = ((id >> 8) & 0x1);
		channel = (id >> 9);
		adapter = (channel >> 1);
	}

	channel = channel % 2;

	// check no logic device is taken in exclusive
	// for the physical drive
	// and the drive is not taken exclusive
	if(ata_adapters[adapter].channels[channel].devices[dev].ldev.exclusive || (exclusive && ata_adapters[adapter].channels[channel].devices[dev].ldev.owners > 0)) return 1;

	it = get_head_position(&ata_adapters[adapter].channels[channel].devices[dev].logic_devs);

	if(ldevid == -1)
	{
		while(it != NULL)
		{
			ldev = (struct logic_device *)get_next(&it);
			
			if(ldev->owners > 0) return 1; // fail!! (physical device cannot be take on exclusive if a logic device is taken)
		}

		ata_adapters[adapter].channels[channel].devices[dev].ldev.ownerid[ld->owners] = task;
		ata_adapters[adapter].channels[channel].devices[dev].ldev.owners++;
		ata_adapters[adapter].channels[channel].devices[dev].ldev.exclusive = exclusive;
	}
	else
	{
		if(ldev->exclusive || (exclusive && ldev->owners > 0)) return 1;	

		ldev->ownerid[ldev->owners] = task;
		ldev->owners++;
		ldev->exclusive = exclusive;
	}

	return 0;
}

int ldev_stdfree(int id, int task)
{
	struct logic_device *ldev;
	
	ldev = ldev_get(id);

	if(ldev == NULL) return 1;

	if(ldev->exclusive)
	{
		ldev->exclusive = 0;
		ldev->owners = 0;
		return 0;
	}

	int i = 0;

	while(i < ldev->owners){ if(ldev->ownerid[i] == task) break; i++; }

	if(i == ldev->owners) return 0;
	
	while(i < ldev->owners-1)
	{
		ldev->ownerid[i] = ldev->ownerid[i+1];
		i++;
	}
	ldev->owners--;

	return 0;
}

/* This function will go thorugh the device partitions scheme (if ATA device, atapi will be treated different)*/
int ldev_find(struct ata_channel *channel, int dev)
{
	/* Create physical logic device */
	ldev_init(channel, dev);	

	init(&channel->devices[dev].logic_devs);
	init(&channel->devices[dev].ptables);
	
	/* Parse partitions, and create logical devices */
	if(channel->devices[dev].reg_config_info == REG_CONFIG_TYPE_ATA)
	{
		/* Logic devices numbering: 
			Ok... I'll make this simple, we can have many channels, with 2 devs at most, with different ids, and many
			partitions inside each channel.
			We will allow up to FF partitions and 10 channels, and a logic device id will be calculated by:

			channel->id << 9 + dev << 8 + partitionid;

			and partition id will be (partition table id * 4 + entry on partition table)
		*/
		
		/* Read partition table from the device */
		// begin at sector 0
		int ext = 0;
		int partitionid = 0;
		unsigned int extendedlba = 0;
		do
		{
			if(channel_read(channel, dev, (unsigned int)channel->reg_buffer, extendedlba, 1))
			{
				return 1;
			}

			// check partition table entries
			struct partition_table_sector *pt_sect = (struct partition_table_sector*)channel->reg_buffer;

			// create ptinfo struct
			struct ptinfo *ptinf = (struct ptinfo*)malloc(sizeof(struct ptinfo));

			ptinf->lba = extendedlba;

			int i = 0;
			for(i = 0;i < 4; i++)
			{
				ptinf->entries[i] = pt_sect->entries[i];
			}

			add_tail(&channel->devices[dev].ptables, ptinf);


			for(i = 0;i < 4; i++)
			{
				/* Is partition valid? */
				if(pt_sect->entries[i].type == 0x00) continue;

				if(pt_sect->entries[i].type == 0x0F)
				{
					/* Extended partition */
					ext = 1;
					extendedlba = pt_sect->entries[i].lba;
				}
				else
				{
					struct logic_device *ldev = (struct logic_device*)malloc(sizeof(struct logic_device));

					ldev->adapter = channel->data_adapterid;	// adapter on which the channel is located
					ldev->channel = channel->id;				// ata channel for the device
					ldev->drive = dev;							// device on the ata channel (0 or 1)

					ldev->id = (channel->id << 9) | (dev << 8) | partitionid;;		
					ldev->owners = 0;		// current owners
					ldev->exclusive = 0;
					ldev->metadata_end = 1;
					ldev->volume_type = NULL;
					ldev->ptype = pt_sect->entries[i].type;
					ldev->bootable = (pt_sect->entries[i].bootable == 0x80);
					ldev->ptlba = ptinf->lba;
					ldev->ptentry = i;

					int j;
					for(j = 0;j<MAX_OWNERS; j++) ldev->ownerid[j] = -1;

					/* see if CHS is corect, if so, check it's the same as the lba */
					if(pt_sect->entries[i].start_CHS[0] >= 0xFE && pt_sect->entries[i].start_CHS[1] == 0xFF && pt_sect->entries[i].start_CHS[2] == 0xFF)
					{
						// use only lba
						ldev->slba = pt_sect->entries[i].lba;		// starting lba for the logic device
						ldev->elba = ldev->slba + pt_sect->entries[i].size - 1;	// ending lba for the logic device
					}
					else
					{
						// compute lba and check it's the same as the one on the entry
						// FIXME: this should check CHS and lba match...
						ldev->slba = pt_sect->entries[i].lba;					// starting lba for the logic device
						ldev->elba = ldev->slba + pt_sect->entries[i].size - 1;	// ending lba for the logic device
					}

					// add logic device to the list
					add_tail(&channel->devices[dev].logic_devs, ldev);

					if(pt_sect->entries[i].type == 0x7F)
					{
						/* Alt-OS-Development Partition Specification 
						Let's fetch aditional data specified on the TIS.
						*/
						ldev->metadata_end = 1;

						/* Fetch first partition sector onto the second block of the buffer
						so we don't step over the current partition table. */
						if(channel_read(channel, dev, (unsigned int)channel->reg_buffer + 512, ldev->slba, 1))
						{
							return 1;
						}

						/* Get the TIS pointer */
						// FIXME: On the specification it is said not to load the TIS from offset 508
						// because of sector size, this should be changed.
						unsigned short tisp = *((unsigned short*)(channel->reg_buffer + 508));
						unsigned int tis_off = tisp % 512;
						tisp = (unsigned short)(tisp / 512);

						/* fetch the TIS volume name and tag */
						if(channel_read(channel, dev, (unsigned int)channel->reg_buffer + 512, ldev->slba + tisp, 1))
						{
							return 1;
						}

						/* Check tis tag */
						if( (channel->reg_buffer + 512 + tis_off)[0] == 0xA0 && (channel->reg_buffer + 512 + tis_off)[0] == 0xDF )
						{
							/* Ok TIS tag is ok... fetch volume type name */
							char *vol_type = (char*)(channel->reg_buffer + 512 + tis_off + 2);
							int len = 0;
							while(vol_type != '\0' && len < 256) { vol_type++; len++; }

							if(len == 256)
							{
								len++;
								vol_type++;
							}

							ldev->volume_type = (char*)malloc(len);
			
							ldev->volume_type[len] = '\0';
							len--;
							vol_type--;
							while(len >= 0) 
							{ 
								ldev->volume_type[len] = *vol_type;
								vol_type--; 
								len--; 
							}
						}

						/*FIXME: fetch TIS Named attribute list */						
					}
				}

				partitionid++;

			}

		}while(ext);	// keep scanning when an extended partition is found.
	}

	return 0;
}

void ldev_init(struct ata_channel *channel, int dev)
{
	int i = 0;

	struct logic_device *ldev = &channel->devices[dev].ldev;

	ldev->adapter = channel->data_adapterid;	// adapter on which the channel is located
	ldev->channel = channel->id;				// ata channel for the device
	ldev->drive = dev;							// device on the ata channel (0 or 1)

	ldev->id = ((channel->id << 1) | dev) | STDDEV_PHDEV_FLAG;		
	ldev->owners = 0;		// current owners
	ldev->exclusive = 0;
	ldev->metadata_end = 0;
	ldev->slba = 0;
	ldev->elba = channel->devices[dev].sector_count - 1;

	for(i = 0; i < MAX_OWNERS; i++)
		ldev->ownerid[i] = -1;
}
