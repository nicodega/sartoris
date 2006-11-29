
#include "types.h"
#include "task_thread.h"
#include "command.h"
#include "signals.h"

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

void exception_signal(UINT16 task_id, UINT16 thread_id, UINT16 exception)
{
	struct pm_task *tsk = tsk_get(task_id);
	struct event_cmd evt;
	struct thr_signal signal;
	
	if(tsk->exeptions.exceptions_port == 0xFFFF)
		fatal_exception(task_id, exception);
	
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

