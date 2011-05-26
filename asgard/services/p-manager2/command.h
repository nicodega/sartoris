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


#include "io.h"
#include <services/pmanager/services.h>
#include "types.h"

#ifndef COMMANDH
#define COMMANDH

struct pending_command
{
	struct pm_msg_generic msg;
	UINT16 sender_task;
	struct pending_command *next;
	struct pending_command *prev;
} PACKED_ATT;

struct 
{
	struct pending_command *first;
	struct pending_command *last;
	UINT32 total;
} command_queue;

struct command_info
{
	UINT16 task_id;
	INT32 ret_value;           /* Return value used when closing the file, 
                               for finished msg 				                     */
	UINT32 command_sender_id;  /* If the task is being destroyed this will hold the 
                               Id of the task who requested it                       */
	UINT16 command_req_id;     /* Request Id for the last message                    */
	UINT16 command_ret_port;   /* Return Port for the Destroy Response (Ret value 
                               will be sent)                                         */
	struct pending_command *executing;	/* Command being executed for this task      */
	INT32 (*callback)(struct pm_task *task, INT32 ioret);	// Callback function for task commands.
} PACKED_ATT;

/* Timeout (in ticks) for service shutdown */
#define SHUTDOWN_TIMEOUT 5000

void cmd_info_init(struct command_info *cmd_inf);
void cmd_process_msg();

void cmd_init();
BOOL shuttingDown();
BOOL cmd_shutdown_step();
void shutdown_tsk_unloaded(UINT16);

void cmd_begin_debug(UINT16 dbg_task, struct pm_msg_dbgattach *msg);
void cmd_end_debug(UINT16 dbg_task, struct pm_msg_dbgend *msg);
void cmd_resumethr_debug(UINT16 dbg_task, struct pm_msg_dbgtresume *msg);
void cmd_load_library(struct pm_msg_loadlib *msg, UINT16 loader_task);
void cmd_create_task(struct pm_msg_create_task *msg, struct pm_task *ctask);
void cmd_create_thread(struct pm_msg_create_thread *msg, UINT16 creator_task_id);
INT32 cmd_task_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret);
void cmd_inform_result(void *msg, UINT16 task_id, UINT16 status, UINT16 new_id, UINT16 new_id_aux);
void cmd_info_init(struct command_info *cmd_inf);

/* Command Queue */
BOOL cmd_queue_add(struct pm_msg_generic *msg, UINT16 sender);
void cmd_queue_remove(struct pending_command *cmd);
void cmd_queue_remove_bytask(struct pm_task *task);

#endif
