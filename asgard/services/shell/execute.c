/*
*	Shell Service.
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

#include "shell_iternals.h"

/*
This function will generate a unique process id.
*/
int gen_process_id()
{
	int taken, candidate = -1;
	struct console_proc_info *pinf;

	/* 
		We will go through the processes list discarding used ids, as 
		well as ids with pipes pending.
	*/
	do
	{
		CPOSITION it = get_head_position(&running);
		
		candidate++;
		taken = 0;

		while(it != NULL)
		{
			pinf = (struct console_proc_info *)get_next(&it);

			if(pinf->id == candidate || pinf->piped_to_task == candidate)
			{
				taken = 1;
				break;
			}
		}

	}while(taken);

	return candidate;
}

struct console_proc_info *execute(int running_term, int console, char *command_path, char *cmd_name, char *params, int pipe, int pipeis_stdin)
{
	char *stdoutfn = NULL, *stdinfn = NULL, *stderrfn = NULL;
	struct pm_msg_create_task   msg_create_tsk;
	struct pm_msg_response      msg_res;
	int sender_id;
	int path_smo_id;

	// setup a new process infor
	struct console_proc_info *pinf = (struct console_proc_info *)malloc(sizeof(struct console_proc_info));

	pinf->id = gen_process_id();
	pinf->maps.mapstdout = 0;
	pinf->maps.mapstdin = 0;
	pinf->maps.mapstderr = 0;
	pinf->stdout = NULL;
	pinf->stdin = NULL;
	pinf->stderr = NULL;
	pinf->cmd_name = cmd_name;
	pinf->running_term = running_term;
	pinf->stderrout = 0;
    pinf->flags = PINF_FLAG_NONE;

	if((pipe && pipeis_stdin) || !pipe)
	{
		pinf->console = console;
	}
	else
	{
		pinf->console = -1;
	}
    if(pipeis_stdin)
	    pinf->flags |= PINF_FLAG_STDIN_PIPED;
	pinf->piped_to_task = -1;
	
	// see if stdout is being redirected
	int errindex = first_index_of_str(params, "2>", 0);
	int outindex = first_index_of_str(params, "1>",0);
	int inindex = first_index_of_str(params, "0<", 0);
	int outappend = 0;

	if(inindex == -1) inindex = first_index_of(params, '<', 0); // see if its in it's short way
	if(outindex == -1)
		outindex = first_index_of_str(params, ">>", 0);

	if(outindex == -1)
	{
		// see if it's on it's short form
		outindex = first_index_of(params, '>', 0);

		if(outindex == errindex && outindex != -1) // this is beacuse whe might have goten the stderr
			outindex = first_index_of(params, '>', outindex+2);
	}
	else
	{
		outappend = 1;
	}

	if(outindex != -1) params[outindex] = '\0';
	if(errindex != -1) params[errindex] = '\0';
	if(inindex != -1) params[inindex] = '\0';

	if(outindex != -1)
	{
		// if piped and it's stdout is mapped fail
		if(pipe && !pipeis_stdin)
		{
			term_print(running_term, "Cannot redirect stdout when process is on the left side of a pipe.\n");
			free(pinf);
		 	return NULL;
		}

		if(params[outindex+1] == '>')
		{
			stdoutfn = build_path(trim(&params[outindex+2]), running_term);
		}
		else	
		{
			stdoutfn = build_path(trim(&params[outindex+1]), running_term); // short version
		}
			
		trim(params);
		pinf->maps.mapstdout = 1;
	}

	// see if stderr is being redirected
	if(errindex != -1)
	{
		if(params[errindex+2] == '&' && params[errindex+2] == '1')
		{
			// mapped to the same as stdout
			stderrfn = stdoutfn;
			pinf->stderrout = 1;
		}
		else
		{
			stderrfn = build_path(trim(&params[errindex+2]), running_term);
		}
		
		trim(params);
		if((stderrfn == stdoutfn && stderrfn != NULL) || stderrfn != stdoutfn)
		{
			pinf->maps.mapstderr = 1;
		}
	}

	// see if stdin is being redirected	
	if(inindex != -1)
	{
		// check stdin is not being redirected when piped
		if(pipe && pipeis_stdin)
		{
			if(stdoutfn != NULL) free(stdoutfn);
			if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
			term_print(running_term, "Cannot redirect stdin when process is on the right end of a pipe.\n");
		 	free(pinf);
		 	return NULL;
		}

		if(params[inindex+1] == '<')
		{
			stdinfn = build_path(trim(&params[inindex+2]), running_term);
		}
		else
		{
			trim(&params[inindex+1]);

			stdinfn = build_path(trim(&params[inindex+1]), running_term); // short version
		}

		if(stdinfn == NULL)
		{
			if(stdoutfn != NULL) free(stdoutfn);
			if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
			term_print(running_term, "stdin File not found.\n");
		 	free(pinf);
		 	return NULL;
		}

		trim(params);
		pinf->maps.mapstdin = 1;
	}

	if(pipe)
	{
		if(pipeis_stdin)
			pinf->maps.mapstdin = 1;
		else
			pinf->maps.mapstdout = 1;
	}

	trim(params);

	// rebuild params
	if(len(cmd_name) != len(params))
	{
		// find where the original command ended
		int i = 0;

		while(params[i] != ' ' && params[i] != '\0') i++;
		if(params[i] != '\0')
		{
			istrcopy(cmd_name, params, 0);
			params[len(cmd_name)] = ' '; // remove \0 from cmd_name
			int j = len(cmd_name)+1;
			while(params[i] == ' ') i++;

			while(params[i] != '\0')	
			{
				params[j] = params[i];
				i++;
				j++;
			}
			params[j] = '\0';
		}
		else
		{
			istrcopy(cmd_name, params, 0);
		}
	}

	path_smo_id = share_mem(PMAN_TASK, command_path, len(command_path)+1, READ_PERM);

	msg_create_tsk.pm_type = PM_CREATE_TASK;
	msg_create_tsk.req_id = running_term;
	msg_create_tsk.response_port = PM_TASK_ACK_PORT;
	msg_create_tsk.flags = 0;
	msg_create_tsk.new_task_id = -1;
	msg_create_tsk.path_smo_id = path_smo_id;
    msg_create_tsk.param = SHELL_INITRET_PORT;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_tsk);

    int finished = 0;
	// here we are going to wait for response on PM_TASK_ACK_PORT.
	// as finished commands might be sent here too, if it's a FINISHED
	// command, we will process it and continue waiting for ack from PMAN.
	for(;;)
	{
		if(get_msg_count(PM_TASK_ACK_PORT) != 0)
		{
			get_msg(PM_TASK_ACK_PORT, &msg_res, &sender_id);
			
			if(sender_id != PMAN_TASK)
			{
				continue;
			}

			if(msg_res.pm_type == PM_TASK_FINISHED)
			{
			    task_finished(msg_res.new_id, ((struct pm_msg_finished*)&msg_res)->ret_value);
				finished = 1;
			   	reschedule();
			}
			else if(msg_res.pm_type == PM_CREATE_TASK)
			{
				break;
			}
		}
		else
		{
			reschedule();
		}
	}

    claim_mem(path_smo_id); 

	if (msg_res.status != PM_OK) {
		term_print(running_term, "task creation failed\n");
		if(stdinfn != NULL) free(stdinfn);
		if(stdoutfn != NULL) free(stdoutfn);
		if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
		free(pinf);
		return NULL;
	}
    else if (finished)
    {
        return NULL;
    }

	pinf->task = msg_res.new_id;

	if(pinf->maps.mapstdout && !(!pipeis_stdin && pipe))
	{
		// create a file pipe
		pinf->stdout = open_fpipe(pinf->task, stdoutfn, ((outappend)? IOLIB_FPIPEMODE_APPEND : IOLIB_FPIPEMODE_WRITE));
		if(pinf->stdout == NULL)
		{
			if(stdinfn != NULL) free(stdinfn);
			if(stdoutfn != NULL) free(stdoutfn);
			if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
			destroy_tsk(pinf->task);
			term_print(running_term, "could not open stdout file\n");
			free(pinf);
		 	return NULL;
		}
		pinf->maps.stdout = *pinf->stdout;
	}

	if(pinf->maps.mapstdin && !(pipeis_stdin && pipe))
	{
		// create a file pipe
		pinf->stdin = open_fpipe(pinf->task, stdinfn, IOLIB_FPIPEMODE_READ);
		if(pinf->stdin == NULL)
		{
			if(stdinfn != NULL) free(stdinfn);
			if(stdoutfn != NULL) free(stdoutfn);
			if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
			destroy_tsk(pinf->task);
			if(pinf->stdout != NULL) fclose(pinf->stdout);
			term_print(running_term, "could not open stdin file\n");
			free(pinf);
		 	return NULL;
		}
		pinf->maps.stdin = *pinf->stdin;
	}
	if(pinf->maps.mapstderr)
	{
		// create a file pipe
		if(stderrfn == stdoutfn)
		{
			pinf->stderr = pinf->stdout;
		}
		else
		{
			pinf->stderr = open_fpipe(pinf->task, stderrfn, IOLIB_FPIPEMODE_WRITE);
			if(pinf->stderr == NULL)
			{
				if(stdinfn != NULL) free(stdinfn);
				if(stdoutfn != NULL) free(stdoutfn);
				if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);
				destroy_tsk(pinf->task);
				if(pinf->stdout != NULL) fclose(pinf->stdout);
				if(pinf->stdin != NULL) fclose(pinf->stdin);
				term_print(running_term, "could not open stderr file\n");
				free(pinf);
			 	return NULL;
			}
		}
		if(!pinf->stderrout && pipe && !pipeis_stdin) pinf->maps.stderr = *pinf->stderr;
	}

	if(stdinfn != NULL) free(stdinfn);
	if(stdoutfn != NULL) free(stdoutfn);
	if(stderrfn != NULL && stderrfn != stdoutfn) free(stderrfn);

	if(pinf->console != -1)
	{
		// free the terminal
		free_console(pinf->console);

		// set this terminal as being used by other process
		cslown[pinf->console].mode = SHELL_CSLMODE_PROC; // console is taken by the shell
		cslown[pinf->console].process = pinf->task;
	}
	pinf->param_smo = share_mem(pinf->task, params, len(params)+1, READ_PERM);
	
    // Since sartoris 2.0, ports on tasks are initially closed.
    // Now a task will have to send the shell a message telling
    // it the process was loaded and requesting the initmsg.
    // This will be implemented on init.c, but in case a task
    // is compiled as a standalone app, we will account for the case
    // where the task finishes before sending the message.

	// insert pinf onto running list
	add_tail(&running, pinf);

	// send process initialization message
	if(pinf->maps.mapstdout || pinf->maps.mapstdin || pinf->maps.mapstderr)
		pinf->map_smo = share_mem(pinf->task, &pinf->maps, sizeof(struct map_params), READ_PERM);
	else
		pinf->map_smo = -1;
	
	return pinf; // thread will be created on finish_execute
}

int finish_execute(struct console_proc_info *p, FILE *pipe, int pipeis_stdin)
{
	struct pm_msg_create_thread msg_create_thr;
	struct pm_msg_response      msg_res;
	int sender_id;

	if(p == NULL) return FALSE;

	// open mapping pipes
	if(pipe != NULL)
	{
		if(pipeis_stdin)
		{
			p->maps.stdin = *pipe;
			p->stdin = pipe;
		}
		else
		{
			p->maps.stdout = *pipe;
			p->stdout = pipe;
			if(p->stderrout)
			{
				p->maps.stderr = *pipe;
			}
		}
	}

	// create thread
	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = p->running_term;
	msg_create_thr.response_port = PM_THREAD_ACK_PORT;
	msg_create_thr.task_id = p->task;
	msg_create_thr.flags = 0;
	msg_create_thr.interrupt = 0;
	msg_create_thr.entry_point = 0;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	while (get_msg_count(PM_THREAD_ACK_PORT) == 0) reschedule();

	get_msg(PM_THREAD_ACK_PORT, &msg_res, &sender_id);

	if (msg_res.status != PM_THREAD_OK) 
	{
		destroy_tsk(p->task);

		term_print(p->running_term, "Thread creation failed!\n");
		
		task_finished(p->task, -1);
		return FALSE;
	}
	return TRUE;
}

