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
#include "partitions.h"


int process_ioctrl(struct stddev_ioctl_cmd *cmd, int task, struct stddev_ioctrl_res *res)
{
	int ret = 1;
	res->dev_error = STDBLOCKDEVERR_INVALIDIOCTRL;
	res->logic_deviceid = cmd->logic_deviceid;
	res->msg_id = cmd->msg_id;
	res->ret = STDDEV_ERR;
	res->logic_deviceid = cmd->logic_deviceid;

	if(!ldev_valdid(cmd->logic_deviceid) && cmd->request != ATAC_IOCTRL_ENUMDEVS && cmd->request != ATAC_IOCTRL_FINDLDEV)
	{ 
		res->dev_error = STDBLOCKDEVERR_INVALIDDISKNUMBER;
		return 1;
	}

	switch(cmd->request)
	{
		/* TODO: Implement IOCTRL commands! */
		case ATAC_IOCTRL_FINDLDEV:
			ret = ioctrl_finddev((struct atac_ioctl_finddev*)cmd, task, res);
			break;
		case ATAC_IOCTRL_REMOVELDEV:
			ret = ioctrl_removeldev((struct atac_ioctl_removeldev*)cmd, task, res);
			break;
		case ATAC_IOCTRL_CREATELDEV:
			ret = ioctrl_createldev((struct atac_ioctl_createldev*)cmd, task, res);
			break;
		case ATAC_IOCTRL_ENUMDEVS:
			ret = ioctrl_enumdevs((struct atac_ioctl_enumdevs*)cmd, task, res);
			break;
	}

	return ret;
}

int ioctrl_finddev(struct atac_ioctl_finddev *cmd, int task, struct stddev_ioctrl_res *res)
{
	struct atac_find_dev_param params;

	/* Atemt reading params smo */
	if(read_mem(cmd->find_dev_smo, 0 , sizeof(struct atac_find_dev_param), &params) != SUCCESS)
	{
		res->dev_error = ATAC_IOCTRLERR_INVALIDSMO;
		return 1;
	}

	int a, c, d;
	CPOSITION it;

	/* Go through all adapters, and devices on channels sarting from <0,0,0> */
	for(a = 0; a < MAX_ADAPTERS; a++)
	{
		/* Go through all channels */
		for(c = 0; c < 2; c++)
		{
			/* Go through all devices */
			for(d = 0; d < 2; d++)
			{
				/* Go through all logic devices */
				it = get_head_position(&ata_adapters[a].channels[c].devices[d].logic_devs);

				while(it != NULL)
				{
					struct logic_device *ldev = (struct logic_device *)get_next(&it);

					if(ldev->ptype == params.ptype && ((!params.bootable) || (params.bootable && ldev->bootable)))
					{
						if(ldev->ptype == 0x7F)
						{
							/* check volume_type */
							int i = 0;
							while(ldev->volume_type[i] != '\0')
							{
								if(ldev->volume_type[i] != params.strtype[i]) continue;
								i++;
							}
						}
						/* Found device */
						params.ldevid = ldev->id;
						params.start_lba = ldev->slba;
						params.metadata_end = ldev->metadata_end;
						params.size = (ldev->elba - ldev->slba) + 1;

						if(write_mem(cmd->find_dev_smo, 0 , sizeof(struct atac_find_dev_param), &params))
						{
							res->dev_error = ATAC_IOCTRLERR_INVALIDSMO;
							return 1;
						}
						res->dev_error = ATAC_IOCTRLERR_OK;
						res->ret = STDDEV_OK;
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

int ioctrl_removeldev(struct atac_ioctl_removeldev *cmd, int task, struct stddev_ioctrl_res *res)
{
	/* Get logic device and check it's owners */
	struct logic_device *ldev, *ldevrem;
	int i = 0;

	if( cmd->logic_deviceid & STDDEV_PHDEV_FLAG)
	{
		res->dev_error = ATAC_IOCTRLERR_DEVICEINUSE;
		return 1;
	}

	ldev = ldev_get(cmd->logic_deviceid);

	if(ldev == NULL)
	{
		res->dev_error = ATAC_IOCTRLERR_NOTFOUND;
		return 1;
	}

	if(ldev->owners > 0)
	{
		res->dev_error = ATAC_IOCTRLERR_DEVICEINUSE;
		return 1;
	}

	/* Ok device is free. remove it from it's partition table */
	CPOSITION it = get_head_position(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].ptables);
	struct ptinfo *ptinf = NULL;
	struct ptinfo *parent_ptinf = NULL;

	while(it != NULL)
	{
		ptinf = (struct ptinfo*)get_next(&it);
		if(ptinf->lba == ldev->ptlba) break;
		parent_ptinf = ptinf;
	}

	ptinf->entries[ldev->ptentry].type = 0x00;
	ptinf->entries[ldev->ptentry].bootable = 0x00;
	ptinf->entries[ldev->ptentry].start_CHS[0] = 0x00;
	ptinf->entries[ldev->ptentry].start_CHS[1]  = 0x00;
	ptinf->entries[ldev->ptentry].start_CHS[2]  = 0x00;
	ptinf->entries[ldev->ptentry].ending_CHS[0] = 0x00;
	ptinf->entries[ldev->ptentry].ending_CHS[1] = 0x00;
	ptinf->entries[ldev->ptentry].ending_CHS[2] = 0x00;
	ptinf->entries[ldev->ptentry].lba = 0x0;
	ptinf->entries[ldev->ptentry].size = 0x0;	// ok partition is now deleted

	/* If the device is on an extended partition, and it's the last device
	remove the extended partition. */
	if(parent_ptinf != NULL && ptinf->entries[0].type == 0x00 && ptinf->entries[1].type == 0x00 
		&& ptinf->entries[2].type == 0x00)
	{
		/* find extended partition possition, and remove it */
		for(i = ldev->ptentry; i < 4; i++)
		{
			if(ptinf->entries[i].type == 0x0F) break;
		}

		ptinf->entries[i].bootable = 0x00;
		ptinf->entries[i].start_CHS[0] = 0x00;
		ptinf->entries[i].start_CHS[1]  = 0x00;
		ptinf->entries[i].start_CHS[2]  = 0x00;
		ptinf->entries[i].ending_CHS[0] = 0x00;
		ptinf->entries[i].ending_CHS[1] = 0x00;
		ptinf->entries[i].ending_CHS[2] = 0x00;
		ptinf->entries[i].lba = 0x0;
		ptinf->entries[i].size = 0x0;	
		ptinf->entries[i].type = 0x00;	// ok extended partition is now deleted

		ptinf = parent_ptinf;
	}

	wait_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);

	/* Read partition table sector */
	if(channel_read(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, ptinf->lba, 1))
	{
		leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
		return 1;
	}

	/* Preserve changes on disk */
	/*
		NOTE: We have to stop channel activity here, hence we will enter the queue mutex :)
	*/

	/* Write entries on buffer  */
	struct partition_entry *ptent = ((struct partition_entry *)(ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer + 446));

	for(i = ldev->ptentry; i < 3; i++)
	{
		*ptent = ptinf->entries[i];
		ptent++;
	}

	/* Preserve the buffer */
	if(channel_write(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, ptinf->lba, 1))
	{
		leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
		return 1;
	}

	/* Remove logic device */
	it = get_head_position(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].logic_devs);
	CPOSITION cit = NULL;

	while(it != NULL)
	{
		cit = it;
		ldevrem = (struct logic_device*)get_next(&it);
		if(ldevrem->id == ldev->id)
		{
			remove_at(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].logic_devs, cit);
			free(ldev);
			break;
		}
		parent_ptinf = ptinf;
	}

	leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);

	res->dev_error = ATAC_IOCTRLERR_OK;
	res->ret = STDDEV_OK;

	return 0;
}

int ioctrl_createldev(struct atac_ioctl_createldev *cmd, int task, struct stddev_ioctrl_res *res)
{
	struct atac_create_ldev_param params;
	struct logic_device *ldev = NULL, *nldev = NULL;
	int i = 0;

	if( !(cmd->logic_deviceid & STDDEV_PHDEV_FLAG))
	{
		res->dev_error = ATAC_IOCTRLERR_ERR;
		return 1;
	}

	ldev = ldev_get(cmd->logic_deviceid);

	if(ldev == NULL)
	{
		res->dev_error = ATAC_IOCTRLERR_NOTFOUND;
		return 1;
	}

	/* Atemt reading params smo */
	if(read_mem(cmd->create_dev_smo, 0 , sizeof(struct atac_create_ldev_param), &params))
	{
		res->dev_error = ATAC_IOCTRLERR_INVALIDSMO;
		return 1;
	}

	/* Check new partition does not collide with a current partition */
	CPOSITION it = get_head_position(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].logic_devs);

	while(it != NULL)
	{
		struct logic_device *ldev = (struct logic_device *)get_next(&it);

		if((ldev->slba <= params.start_lba && ldev->elba >= params.start_lba) || (ldev->slba <= params.start_lba+params.size && ldev->elba > params.start_lba+params.size))
		{
			res->dev_error = ATAC_IOCTRLERR_ERR;
			return 1;
		}
	}

	/* Get last partition table */
	it = get_tail_position(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].ptables);
	struct ptinfo *ptinf = (struct ptinfo *)get_at(it), *nptinf = NULL;
	
	/* If this partition table is full, fail */
	int fcount = 0, fpos = 5;
	for(i = 0; i < 4; i++)
	{
		if(ptinf->entries[i].type == 0x00)
		{
			if(fpos == 5) fpos = i;
			fcount++;
		}
	}

	if(fcount == 0)
	{
		res->dev_error = ATAC_IOCTRLERR_ERR;
		return 1;
	}

	wait_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);

	/* Now fpos contains the first empty entry on the pt. 
	If there is only one free entry, create an extended partition. */

	struct partition_entry *ptent = NULL;

	/* NOTE: This should be implemented in a different way, but 
	this way is simple. */
	if(fcount == 1)
	{
		ptinf->entries[fpos].type = 0x0F;		// Extended partition
		ptinf->entries[fpos].bootable = 0x00;

		ptinf->entries[fpos].start_CHS[0] = 0xFE;
		ptinf->entries[fpos].start_CHS[1] = 0xFF;
		ptinf->entries[fpos].start_CHS[2] = 0xFF;
		ptinf->entries[fpos].ending_CHS[0] = 0xFE;
		ptinf->entries[fpos].ending_CHS[1] = 0xFF;
		ptinf->entries[fpos].ending_CHS[2] = 0xFF;

		nptinf->entries[fpos].lba = params.start_lba;
		nptinf->entries[fpos].size = params.size;

		params.size--;
		params.start_lba++;

		nptinf = (struct ptinfo*)malloc(sizeof(struct ptinfo));

		nptinf->lba = params.start_lba-1;

		for(i = 0; i < 4; i++)
		{
			nptinf->entries[i].type == 0x00;
		}

		fpos = 0;

		/* add ptable to the list */
		add_tail(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].ptables, nptinf);

		/* read partition table sector and preserve changes */
		if(channel_read(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, ptinf->lba, 1))
		{
			leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
			return 1;
		}

		ptent = ((struct partition_entry *)(ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer + 446));

		for(i = 0; i < 4; i++)
		{
			*ptent = ptinf->entries[i];
			ptent++;
		}

		/* Preserve the buffer */
		if(channel_write(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, ptinf->lba, 1))
		{
			leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
			return 1;
		}

	}
	else
	{
		nptinf = ptinf;
	}

	/* If it's an alt os partition, create the TIS, and set TIS pointer on first partition sector. */
	if(params.ptype == 0x7F)
	{
			/* Calculate and write tisp */
			unsigned short *tisp =  (unsigned short*)(ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer + 508);

			*tisp = 508 - (256 + 2 + 1);
	
			unsigned char *tis = (ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer + *tisp);

			/* Write tag */
			tis[0] = 0xA0;
			tis[1] = 0xDF;
			
			/* Write Name */
			i = 0;
			while(params.strtype[i] != '\0' && i < 256)
			{
				(tis + 2)[i] = params.strtype[i];
				i++;
			}
			(tis + 2)[i] = '\0';

			/* Write NAL */
			*(tis + 256 + 2) = 0x00;

			/* Preserve sector */
			if(channel_write(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, params.start_lba, 1))
			{
				leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
				return 1;
			}

			params.start_lba++;
			params.size--;
	}

	nptinf->entries[fpos].type = params.ptype;	// Extended partition
	nptinf->entries[fpos].bootable = ((params.bootable)? 0x80 : 0x00);

	nptinf->entries[fpos].start_CHS[0] = 0xFE;
	nptinf->entries[fpos].start_CHS[1] = 0xFF;
	nptinf->entries[fpos].start_CHS[2] = 0xFF;
	nptinf->entries[fpos].ending_CHS[0] = 0xFE;
	nptinf->entries[fpos].ending_CHS[1] = 0xFF;
	nptinf->entries[fpos].ending_CHS[2] = 0xFF;

	nptinf->entries[fpos].lba = params.start_lba;
	nptinf->entries[fpos].size = params.size;

	/* read partition table sector and preserve changes */
	if(channel_read(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, nptinf->lba, 1))
	{
		leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
		return 1;
	}

	ptent = ((struct partition_entry *)(ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer + 446));

	for(i = 0; i < 4; i++)
	{
		*ptent = nptinf->entries[i];
		ptent++;
	}
	
	/* Preserve the buffer */
	if(channel_write(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, (unsigned int)ata_adapters[ldev->adapter].channels[ldev->channel].reg_buffer, nptinf->lba, 1))
	{
		leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);
		return 1;
	}

	/* Create logic device */
	nldev = (struct logic_device*)malloc(sizeof(struct logic_device));

	nldev->adapter = ldev->adapter;				// adapter on which the channel is located
	nldev->channel = ldev->channel;				// ata channel for the device
	nldev->drive = ldev->drive;					// device on the ata channel (0 or 1)

	nldev->id = (ldev->channel << 9) | (ldev->drive << 8) | (length(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].ptables) * 4 + fpos);		
	nldev->owners = 0;		
	nldev->exclusive = 0;
	nldev->metadata_end = 1;
	nldev->ptype = params.ptype;
	nldev->bootable = params.bootable;
	nldev->ptlba = nptinf->lba;
	nldev->ptentry = fpos;
	nldev->slba = params.start_lba;				// starting lba for the logic device
	nldev->elba = nldev->slba + params.size - 1;	// ending lba for the logic device

	if(params.ptype == 0x7F)
	{
		i = 0;
		while(params.strtype[i] != '\0' && i < 256)
		{
			i++;
		}
		nldev->volume_type = (char*)malloc(i);

		i = 0;
		while(params.strtype[i] != '\0' && i < 256)
		{
			nldev->volume_type[i] = params.strtype[i];
			i++;
		}
	}
	else
	{
		nldev->volume_type = NULL;
	}

	/* Write new id */
	params.new_id = nldev->id;

	write_mem(cmd->create_dev_smo, 0 , sizeof(struct atac_create_ldev_param), &params);

	/* Insert logic device on the list */
	add_tail(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].logic_devs, nldev);

	leave_mutex(&ata_adapters[ldev->adapter].channels[ldev->channel].queue.queue_mutex);

	res->dev_error = ATAC_IOCTRLERR_OK;
	res->ret = STDDEV_OK;

	return 0;
}

int ioctrl_enumdevs(struct atac_ioctl_enumdevs *cmd, int task, struct stddev_ioctrl_res *res)
{
	struct atac_enum_dev_param params;
	struct logic_device *ldev = NULL;

	/* Atemt reading params smo */
	if(read_mem(cmd->enum_dev_smo, 0 , sizeof(struct atac_enum_dev_param), &params))
	{
        res->dev_error = ATAC_IOCTRLERR_INVALIDSMO;
		return 1;
	}

	unsigned short id = params.continuation;

	/* Devices loop will go through physical, and then logical devices. */
	int i = -1, a = 0, c = 0, d = 0, ld = 0;

	for(a = 0; a < MAX_ADAPTERS && i < id; a++)
	{
		for(c = 0; c < 2 && i < id; c++)
		{
			for(d = 0; d < 2 && i < id; d++)
			{
				if(ata_adapters[a].channels[c].devices[d].reg_config_info != REG_CONFIG_TYPE_NONE && ata_adapters[a].channels[c].devices[d].reg_config_info != REG_CONFIG_TYPE_UNKN)
				{   
					ldev = &ata_adapters[0].channels[0].devices[0].ldev;		
					i++;

					if(i != id)
					{
						/* go through logic devices*/
						CPOSITION it = get_head_position(&ata_adapters[ldev->adapter].channels[ldev->channel].devices[ldev->drive].logic_devs);

						while(it != NULL && i < id)
						{
							ldev = (struct logic_device *)get_next(&it);
							i++;
						}
					}
				}
			}
		}	
	}

	if(i != id)
	{
		/* end of list */
		res->dev_error = ATAC_IOCTRLERR_ENUMFINISHED;
		return 1;
	}
    
    /* Write device info */	
	params.devinf.id = ldev->id;
	params.devinf.ptype = ldev->ptype;
	params.devinf.bootable = ldev->bootable;
	params.devinf.start_lba = ldev->slba;
	params.devinf.size = (ldev->elba - ldev->slba) + 1;
	params.devinf.pid = (((ldev->channel * ldev->adapter) << 1) | ldev->drive) | STDDEV_PHDEV_FLAG;
	params.devinf.metadata_end = ldev->metadata_end;

	/* Copy fs name if alt os type name */
	if(params.devinf.ptype == 0x7F)
	{
		i = 0;
		while(ldev->volume_type[i] != '\0')
		{
			params.devinf.strtype[i] = ldev->volume_type[i];
			i++;
		}
	}

	/* Point to next device */
	params.continuation++;

	/* Write device info an continuation */
	if(write_mem(cmd->enum_dev_smo, 0 , sizeof(struct atac_enum_dev_param), &params) < 0)
    {
        res->dev_error = ATAC_IOCTRLERR_INVALIDSMO;
		return 1;
    }

	res->dev_error = ATAC_IOCTRLERR_OK;
	res->ret = STDDEV_OK;

	return 0;
}



