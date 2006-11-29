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

void queue_init(struct command_queue *queue)
{
	init_mutex(&queue->queue_mutex);
	init(&queue->queue[0]);
	init(&queue->queue[1]);
	queue->next_dev = 0;
	queue->ttl_pos[0] = queue->ttl_pos[1] = 0;
}

struct device_command *queue_getnext(struct command_queue *queue, int channelid)
{
	struct device_command *cmd = NULL;
	int dev = -1;

	wait_mutex(&queue->queue_mutex);

	// If the device on the channel with the same ID as last_dev has
	// something waiting on the queue, it will have priority and be executed.
	// once it's executed, last_dev will switch to the other device.
	// If current device has no commands, the turn will be given to the other device.
	if(length(&queue->queue[queue->next_dev]) != 0)
	{
		dev = queue->next_dev;
		queue->next_dev = (queue->next_dev)? 0 : 1;
	}
	else if(length(&queue->queue[(queue->next_dev)? 0 : 1]) != 0)
	{
		/* try the other dev */
		dev = (queue->next_dev)? 0 : 1;
	}

	if(dev == -1)
	{
		/* If there are no commands queued Wait for signal (we will be signaled when there are pending messages on our queue) */
		SIGNALHANDLER sigh = wait_signal_async(CHANNEL_COMMAND_EVENT, get_current_task(), SIGNAL_TIMEOUT_INFINITE, channelid, SIGNAL_PARAM_IGNORE, NULL, NULL);
			
		leave_mutex(&queue->queue_mutex);

		/* wait for the signal */
		while(check_signal(sigh, NULL, NULL) == 0) { reschedule(); }

		discard_signal(sigh);

		wait_mutex(&queue->queue_mutex);
		
		if(length(&queue->queue[queue->next_dev]) != 0)
		{
			dev = queue->next_dev;
			queue->next_dev = (queue->next_dev)? 0 : 1;
		}
		else if(length(&queue->queue[(queue->next_dev)? 0 : 1]) != 0)
		{
			/* try the other dev */
			dev = (queue->next_dev)? 0 : 1;
		}
	}

	CPOSITION it = get_head_position(&queue->queue[dev]);

	cmd = (struct device_command*)get_at(it);

	remove_at(&queue->queue[dev], it);

	/* decrement ttl on all items */
	it = get_head_position(&queue->queue[dev]);
	int i = 0;
	queue->ttl_pos[dev] = 0;
	while(it != NULL)
	{
		struct device_command *cmd = (struct device_command*)get_next(&it);

		cmd->ttl--;

		if(cmd->ttl == 0) queue->ttl_pos[dev] = i;
		i++;
	}

	leave_mutex(&queue->queue_mutex);

	return cmd;
}

void queue_enqueue(struct ata_channel *channel, int device, struct device_command *cmd)
{
	wait_mutex(&channel->queue.queue_mutex);

	/* Use last_task history in order to decide where the item will be inserted.
	Also use command possition.
	*/
	int ttl;

	CPOSITION it = get_insert_pos(&channel->queue, device, cmd->task, cmd->block_msg.pos, &ttl);

	cmd->ttl = ttl;

	if(it == NULL)
		add_tail(&channel->queue.queue[device], cmd);
	else
		insert_before(&channel->queue.queue[device], it, cmd);

	if(length(&channel->queue.queue[device]) == 1 && length(&channel->queue.queue[(device - 1) & 0x1]) == 0)
	{
		send_event(get_current_task(), CHANNEL_COMMAND_EVENT, channel->id, 0, 0, 0);		
	}

	leave_mutex(&channel->queue.queue_mutex);
}

/* This function will return possition before wich the item must be inserted. */
CPOSITION get_insert_pos(struct command_queue *queue, int dev, int task, unsigned int lba, int *ttl)
{
	CPOSITION it = get_tail_position(&queue->queue[dev]), rit;

	*ttl = QUEUE_ITEM_TTL;

	int ttl_pos = length(&queue->queue[dev]) - queue->ttl_pos[dev];
	
	/* Initially the item will be placed at the list tail */

	int range = QUEUE_REORDER_RANGE;

	/* Now reorder item possition based on lba */
	range = QUEUE_REORDER_RANGE;

	rit = it;
	/* reorder within range */
	while(it != NULL && range > 0)
	{
		struct device_command *cmd = (struct device_command*)get_at(it);

		if(ttl_pos == 0)
		{
			return rit;
		}

		/* We must guarantee commands for a same task maintain order */
		if(cmd->block_msg.pos < lba && task != cmd->task)
		{
			ttl_pos--;
			rit = it;
			get_prev(&it);
		}
		else
		{
			return rit;
		}

		range--;
	}	
	
	if(rit == get_head_position(&queue->queue[dev])) return get_head_position(&queue->queue[dev]);

	return NULL;
}


