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

/* This function will enqueue commands on each channel thread */
int process_blockdev(struct stdblockdev_cmd *bmsg, int task, struct stdblockdev_res *bres)
{
	struct device_command *cmd = NULL;

	bres->command = bmsg->command;
	bres->msg_id = bmsg->msg_id;
	bres->dev = bmsg->dev;
	bres->ret = STDBLOCKDEV_ERR;

	/* If channel queue is empty, we will generate an event, else, no event will be generated */
	
	/* get logic device */
	struct logic_device *ldev = ldev_get(bmsg->dev);

	if(ldev == NULL)
	{
		return 0;
	}

	if(bmsg->command == BLOCK_STDDEV_INFO)
	{
		struct stdblockdev_devinfo_cmd *dinfmsg = (struct stdblockdev_devinfo_cmd *)bmsg;

		struct blockdev_info0 devinf;
		devinf.max_lba = ldev->elba - ldev->slba;
		devinf.media_size = (devinf.max_lba + 1) * 512;

		if(bmsg->dev & STDDEV_PHDEV_FLAG)
		{
			devinf.metadata_lba_end = 0; // allow access to all sectors for rw.
		}
		else
		{
			devinf.metadata_lba_end = ldev->metadata_end; // ignore metadata for writes.
		}

		write_mem(dinfmsg->devinf_smo, 0, sizeof(struct blockdev_info0), &devinf);

		// send ok response
		bres->ret = STDBLOCKDEV_OK;

		return 0; // this will force send_msg on atac.c
	}

	/* If this is a write command, logic device id is not a physical 
	device and lba is lower than metadata start lba, fail. */
	if(!(bmsg->dev & STDDEV_PHDEV_FLAG) && (bmsg->command == BLOCK_STDDEV_WRITE || bmsg->command == BLOCK_STDDEV_WRITEM) && bmsg->pos < ldev->metadata_end)
	{
print("tried to write metadata on a logic device",0);
		return 0;
	}
	
	/* build queue message */
	cmd = (struct device_command *)malloc(sizeof(struct device_command));

	if(cmd == NULL) return 0;

	cmd->ldev = ldev;
	cmd->task = task;
	cmd->block_msg = *bmsg;

	/* enqueue message */
	queue_enqueue(&ata_adapters[ldev->adapter].channels[ldev->channel], ldev->drive, cmd);

	return 1;
}

