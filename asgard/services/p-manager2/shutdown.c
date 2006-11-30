
#include "ports.h"
#include "types.h"
#include "command.h"
#include "task_thread.h"
#include <services/stds/stdservice.h>
#include <services/pmanager/services.h>
#include <sartoris/syscall.h>

extern BOOL shutting_down;
UINT32 shutdown_stage;
UINT32 current_proc_count;
UINT32 current_proc_ack;
UINT32 timeout;

BOOL shuttingDown()
{
	INT32 res[4], task_id;

	if(!shutting_down)
	{
		/* eat any messages sent to this port. */
		while(get_msg_count(SHUTDOWN_PORT))
		{
			get_msg(SHUTDOWN_PORT, res, &task_id);
		}
	}
	return shutting_down;
}

/* Shutdown Procedure */
BOOL cmd_shutdown_step()
{
	struct stdservice_cmd sht_cmd;
	INT32 task_id;
	UINT32 i;

	/* Shutdown will be carried in this order:
		- Send Kill message to non service APPS
		- Send kill message to non system services
		- Send Kill message to system services
	*/
	sht_cmd.command = STDSERVICE_DIE;
	sht_cmd.ret_port = SHUTDOWN_PORT;
	
	if(!shutting_down)
	{
		shutting_down = TRUE;

		/* Shutdown has just begun */
		shutdown_stage = 0;
		current_proc_count = 0;
		current_proc_ack = 0;

		/* Count non service APPS and send them a termination msg */
		for(i = 0; i < MAX_TSK; i++)
		{
			struct pm_task *tsk = tsk_get(i);

			if(tsk != NULL && !(tsk->flags & TSK_FLAG_SERVICE))
			{
				if(tsk->state == TSK_NORMAL)
				{
					current_proc_count++;
					send_msg(tsk->id, STDSERVICE_PORT, &sht_cmd);
				}
				else if(tsk->state == TSK_LOADING)
				{
					tsk->state = TSK_NOTHING;

					/* Send a message to creating task so it knows task wont load */
					if(tsk->command_inf.creator_task_id != 0xFFFF)
					{
						struct pm_msg_response msg_ans;
	  
						msg_ans.pm_type = PM_CREATE_TASK;
						msg_ans.req_id  = tsk->command_inf.req_id;
						msg_ans.status  = PM_SHUTINGDOWN;
						msg_ans.new_id  = 0;
						msg_ans.new_id_aux = 0;

						send_msg(tsk->command_inf.creator_task_id, tsk->command_inf.response_port, &msg_ans);  
					}
				}
			}
		}

		/* Begin timeout */
		timeout = ticks + SHUTDOWN_TIMEOUT;
	}
	else
	{
		struct stdservice_res res;
		struct pm_task *tsk = NULL;
		
		/* If timed out, abort */
		if(timeout < ticks)
		{
			shutdown_stage = 0;
			shutting_down = FALSE;
			return FALSE;
		}

		/* Waiting for process shutdown acknowledges */
		while(get_msg_count(SHUTDOWN_PORT))
		{
			if(get_msg(SHUTDOWN_PORT, &res, &task_id) == SUCCESS)
			{
				tsk = tsk_get(task_id);
				if(res.command == STDSERVICE_DIE && res.ret == STDSERVICE_RESPONSE_OK)
				{
					/* Begin Unload */
					tsk->command_inf.command_sender_id = 0;
					tsk->io_finished.callback = cmd_task_destroyed_callback;
					io_begin_close( &tsk->io_event_src );
				}
			}
		}

		if(current_proc_ack == current_proc_count)
		{
			if(shutdown_stage == 2)
			{
				/* Finished closing tasks */
				return TRUE;
			}

			/* Advance Stage */
			shutdown_stage++;
			current_proc_count = 0;
			current_proc_ack = 0;

			/* Count service APPS and send them a termination msg */
			for(i = 0; i < MAX_TSK; i++)
			{
				struct pm_task *tsk = tsk_get(i);

				if(tsk != NULL && (tsk->flags & TSK_FLAG_SERVICE) && (!(tsk->flags & TSK_FLAG_SYS_SERVICE) || shutdown_stage == 2))
				{
					if(tsk->state == TSK_NORMAL)
					{
						current_proc_count++;
						send_msg(tsk->id, STDSERVICE_PORT, &sht_cmd);
					}
					else if(tsk->state == TSK_LOADING)
					{
						tsk->state = TSK_NOTHING;

						/* Send a message to creating task so it knows task wont load */
						if(tsk->command_inf.creator_task_id != 0xFFFF)
						{
							struct pm_msg_response msg_ans;
			
							msg_ans.pm_type = PM_CREATE_TASK;
							msg_ans.req_id  = tsk->command_inf.req_id;
							msg_ans.status  = PM_SHUTINGDOWN;
							msg_ans.new_id  = 0;
							msg_ans.new_id_aux = 0;

							send_msg(tsk->command_inf.creator_task_id, tsk->command_inf.response_port, &msg_ans);  
						}
					}
				}
			}

			/* Begin timeout */
			timeout = ticks + SHUTDOWN_TIMEOUT;			
		}
	}
	return TRUE;
}

void shutdown_tsk_unloaded(UINT16 task)
{
	if(shutting_down)
		current_proc_ack++;	
}