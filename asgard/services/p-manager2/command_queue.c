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

#include "types.h"
#include "command.h"
#include "kmalloc.h"
#include "task_thread.h"

/*
Adds an item at the end of the queue.
*/
BOOL cmd_queue_add(struct pm_msg_generic *msg, UINT16 sender)
{
	struct pending_command *cmd = kmalloc(sizeof(struct pending_command));

	if(cmd == NULL) return FALSE;

	cmd->msg = *msg;
	cmd->sender_task = sender;
	cmd->next = NULL;
	cmd->prev = command_queue.last;
	
	if(command_queue.first == NULL)
	{
		command_queue.first = command_queue.last = cmd;
	}
	else
	{
		command_queue.last->next = cmd;
		command_queue.last = cmd;
	}

	command_queue.total++;

	return TRUE;
}

/*
Removes a command from the queue.
*/
void cmd_queue_remove(struct pending_command *cmd)
{
	if(command_queue.first == NULL) return;

	if(cmd->prev != NULL)
		cmd->prev->next = cmd->next;

	if(cmd->next != NULL)
		cmd->next->prev = cmd->prev;

	if(cmd == command_queue.first)
	{
		command_queue.first = cmd->next;
	}

	if(cmd == command_queue.last)
	{
		command_queue.last = cmd->prev;
	}

	command_queue.total--;
}

void cmd_queue_remove_bytask(struct pm_task *task)
{
	struct pending_command *cmd = command_queue.first, *next;

	while(cmd != NULL)
	{
		next = cmd->next;
		
		if(cmd->sender_task == task->id)
			cmd_queue_remove(cmd);
		
		cmd = next;
	}
}

