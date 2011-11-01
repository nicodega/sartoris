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
    struct pm_thread *thr = NULL;

	if(tsk && tsk->state != TSK_NOTHING && tsk->state != TSK_KILLING) 
	{
		tsk->command_inf.ret_value = ret_value;
		tsk->command_inf.command_sender_id = 0;
		tsk->state = TSK_KILLING;
		tsk->command_inf.command_req_id = -1;
		tsk->command_inf.command_ret_port = -1;

        thr = tsk->first_thread;

        while(thr)
        {
            thr->state = THR_EXCEPTION;
            sch_deactivate(thr);
            thr = thr->next_thread;
        }

        cmd_queue_remove_bytask(tsk);

		tsk->io_finished.callback = cmd_task_fileclosed_callback;
		io_begin_close( &tsk->io_event_src );
	}
}

/*
Send an exception signal.
*/
void exception_signal(UINT16 task_id, UINT16 thread_id, UINT16 exception, ADDR last_exception_addr)
{
	struct pm_task *tsk = tsk_get(task_id);
    struct pm_thread *thr = thr_get(thread_id);
	struct event_cmd evt;
	struct thr_signal signal;
	
	if(tsk == NULL || tsk->exeptions.exceptions_port == 0xFFFF)
    {
		fatal_exception(task_id, exception);
        return;
    }
	
	if(tsk->state != TSK_NOTHING) 
	{
        if(last_exception_addr == tsk->exeptions.last_exception_addr)
        {
            tsk->exeptions.ecount++;
            if(tsk->exeptions.ecount == 3)
            {
                fatal_exception(task_id, exception);
                return;
            }
        }
        else
        {
            tsk->exeptions.ecount = 1;
            tsk->exeptions.last_exception_addr = last_exception_addr;
        }
        
		signal.signal_port = tsk->exeptions.exceptions_port;
		signal.thread = thr_get(thread_id);
		signal.id = 0;
		signal.task = PMAN_TASK;

        if(signal.thread == NULL) return;

		evt.command = EVENT;
		evt.event_type = PMAN_EXCEPTION;
		evt.task = PMAN_GLOBAL_EVENT;
		evt.event_res = (UINT32)exception;

		send_signal_ex(&signal, &evt, SIGNAL_OK, last_exception_addr);
	}
}

