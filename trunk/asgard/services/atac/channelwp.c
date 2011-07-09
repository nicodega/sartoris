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

        switch(cmd->type)
        {
            case DEVICE_COMMAND_BLOCK:
            {
                struct stdblockdev_cmd *bcmd = (struct stdblockdev_cmd *)cmd->msg;
                
		        /* Execute command */
		        switch(bcmd->command)
		        {
			        case BLOCK_STDDEV_READ:
			        case BLOCK_STDDEV_READM:
				        res = wpread(channel, cmd->ldev, bcmd, bcmd->command == BLOCK_STDDEV_READM, ((bcmd->command == BLOCK_STDDEV_READM)? ((struct stdblockdev_readm_cmd*)bcmd)->count : 1) );				
				        break;
			        case BLOCK_STDDEV_WRITE:
			        case BLOCK_STDDEV_WRITEM:
				        res = wpwrite(channel, cmd->ldev, bcmd, bcmd->command == BLOCK_STDDEV_WRITEM, ((bcmd->command == BLOCK_STDDEV_WRITEM)? ((struct stdblockdev_writem_cmd*)bcmd)->count : 1));				
				        break;
		        }

		        bres.command = bcmd->command;
		        bres.msg_id = bcmd->msg_id;
		        bres.dev = bcmd->dev;
                
		        /* Send response */
		        if(res) /* Error */
		            bres.ret = STDBLOCKDEV_ERR;
		        else
		            bres.ret = STDBLOCKDEV_OK;
		        send_msg(cmd->task, bcmd->ret_port, &bres);
                break;
            }
            case DEVICE_COMMAND_IOCTRL:
            {
                struct stddev_ioctrl_res res;
                process_ioctrl((struct stddev_ioctl_cmd *)cmd->msg, cmd->task, &res);
                send_msg(cmd->task, ((struct stddev_ioctl_cmd *)cmd->msg)->ret_port, &res);
                break;
            }
        }
        
        free(cmd);
	}
}

int wpread(struct ata_channel *channel, struct logic_device *ldev, struct stdblockdev_cmd *bcmd, int multiple, int count)
{
	/* Execute read on channel buffer */
	// FIXME: the way this is implemented right now, transfers can't be bigger than
	// channel buffer size

	unsigned int lba = bcmd->pos + ldev->slba;
	if(channel_read(channel, ldev->drive, (unsigned int)channel->reg_buffer, lba, count))
	{
		return 1;
	}

	/* Write on the smo */
	if(write_mem( bcmd->buffer_smo, 0, count * 512, channel->reg_buffer)) return 1;

	return 0;
}

int wpwrite(struct ata_channel *channel, struct logic_device *ldev, struct stdblockdev_cmd *bcmd, int multiple, int count)
{
	/* Execute write on channel buffer */
	// FIXME: the way this is implemented right now, transfers can't be bigger than
	// channel buffer size

	/* read the smo */
	if(read_mem( bcmd->buffer_smo, 0, count * 512, channel->reg_buffer))
	{
		print("channelwp smo reading failed", count);
		return 1;
	}

	unsigned int lba = bcmd->pos + ldev->slba;				
	return channel_write(channel, ldev->drive, (unsigned int)channel->reg_buffer, lba, count);
}

