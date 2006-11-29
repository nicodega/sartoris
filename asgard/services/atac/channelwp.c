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

/* Channel working process. 
This function will be the handler of a given ata channel thread. 
It will select messages from the queue, and execute them.
*/
void channel_wp()
{
	/* Initialization */
	struct ata_channel *channel = channelwp_channel;
	int res = 0;
	struct stdblockdev_res bres;

	channel->initialized = 1;	// signal creator function

	__asm__ ("sti" ::);

	for(;;)
	{
		/* Check for a queued command */
		struct device_command *cmd = queue_getnext(&channel->queue, channel->id);

		res = 0;

		/* Execute command */
		switch(cmd->block_msg.command)
		{
			case BLOCK_STDDEV_READ:
			case BLOCK_STDDEV_READM:					
				res = wpread(channel, cmd, cmd->block_msg.command == BLOCK_STDDEV_READM, ((cmd->block_msg.command == BLOCK_STDDEV_READM)? ((struct stdblockdev_readm_cmd*)&cmd->block_msg)->count : 1) );				
				break;
			case BLOCK_STDDEV_WRITE:
			case BLOCK_STDDEV_WRITEM:
				res = wpwrite(channel, cmd, cmd->block_msg.command == BLOCK_STDDEV_WRITEM, ((cmd->block_msg.command == BLOCK_STDDEV_WRITEM)? ((struct stdblockdev_writem_cmd*)&cmd->block_msg)->count : 1));				
				break;
		}

		bres.command = cmd->block_msg.command;
		bres.msg_id = cmd->block_msg.msg_id;
		bres.dev = cmd->block_msg.dev;

		free(cmd);

		/* Send response */
		if(res)
		{
			/* Error */
			bres.ret = STDBLOCKDEV_ERR;
			/* Build extended error */
			// FIXME: Implement.
		}
		else
		{
			/* Ok */
			bres.ret = STDBLOCKDEV_OK;
		}
		send_msg(cmd->task, cmd->block_msg.ret_port, &bres);
	}
}

int wpread(struct ata_channel *channel, struct device_command *cmd, int multiple, int count)
{
	/* Execute read on channel buffer */
	// FIXME: the way this is implemented right now, transfers can't be bigger than
	// channel buffer size

	unsigned int lba = cmd->block_msg.pos + cmd->ldev->slba;
	if(channel_read(channel, cmd->ldev->drive, (unsigned int)channel->reg_buffer, lba, count))
	{
		return 1;
	}

	/* Write on the smo */
	if(write_mem( cmd->block_msg.buffer_smo, 0, count * 512, channel->reg_buffer)) return 1;

	return 0;
}

int wpwrite(struct ata_channel *channel, struct device_command *cmd, int multiple, int count)
{
	/* Execute write on channel buffer */
	// FIXME: the way this is implemented right now, transfers can't be bigger than
	// channel buffer size

	/* read the smo */
	if(read_mem( cmd->block_msg.buffer_smo, 0, count * 512, channel->reg_buffer))
	{
		print("channelwp smo reading failed", count);
		return 1;
	}

	unsigned int lba = cmd->block_msg.pos + cmd->ldev->slba;				
	return channel_write(channel, cmd->ldev->drive, (unsigned int)channel->reg_buffer, lba, count);
}

