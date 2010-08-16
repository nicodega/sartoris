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
#include "task_thread.h"
#include "command.h"
#include "signals.h"
#include "layout.h"

/*
Finish a task with an exception.
*/
void fatal_exception(UINT16 task_id, INT32 ret_value)
{
	struct pm_task *tsk = tsk_get(task_id);

	if(tsk != NULL && tsk->state != TSK_NOTHING) 
	{
		tsk->command_inf.ret_value = ret_value;
		tsk->command_inf.command_sender_id = 0;
		tsk->state = TSK_KILLING;
		tsk->command_inf.command_req_id = -1;
		tsk->command_inf.command_ret_port = -1;

		tsk->io_finished.callback = cmd_task_destroyed_callback;
		io_begin_close( &tsk->io_event_src );
	}
}
/*
Send an exception signal.
*/
void exception_signal(UINT16 task_id, UINT16 thread_id, UINT16 exception)
{
	struct pm_task *tsk = tsk_get(task_id);
	struct event_cmd evt;
	struct thr_signal signal;
	
	if(tsk->exeptions.exceptions_port == 0xFFFF)
    {
		fatal_exception(task_id, exception);
        return;
    }
	
	if(tsk != NULL && tsk->state != TSK_NOTHING) 
	{
		signal.signal_port = tsk->exeptions.exceptions_port;
		signal.thread = thr_get(thread_id);
		signal.id = 0;
		signal.task = task_id;

		evt.command = EVENT;
		evt.event_type = PMAN_EXCEPTION;
		evt.task = PMAN_GLOBAL_EVENT;
		evt.event_res0 = exception;
		evt.event_res1 = thread_id;
	
		send_signal(&signal, &evt, SIGNAL_OK);
	}
}

