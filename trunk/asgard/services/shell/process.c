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

char txt_gpf[] = "%s -> fatal error: program received general protection fault";
char txt_pgf[] = "%s -> fatal error: program received page fault exception";
char txt_div_zero[] = "%s -> fatal error: program received divide by zero exception";
char txt_ovflw[] =  "%s -> warning: program received overflow exception";
char txt_bound[] =  "%s -> fatal error: program received BOUND range exceeded exception";
char txt_inv_opcode[] = "%s -> fatal error: program received invalid opcode exception";
char txt_dev_not_avl[] = "%s -> fatal error: program received device not available exception";
char txt_stack_fault[] = "%s -> fatal error: program received stack segment fault";
char txt_fp_err[] = "%s -> fatal error: program received floating-point error exception";
char txt_alig_chk[] = "%s -> warning: program received alignment check exception";
char txt_simd_fault[] = "%s -> fatal error: program received SIMD extensions fault";
char txt_pgio_fault[] = "%s -> fatal error: program received Page IO error fault";
char txt_pgrw_fault[] = "%s -> fatal error: program tried to write to a read only page";
char txt_maxaddr_fault[] = "%s -> fatal error: program tried to read/write on an address too high";
char txt_fmapio_fault[] = "%s -> fatal error: program received FMAP IO error fault";
char txt_mem_fault[] = "%s -> fatal error: program received Not Enough Memory fault";
char txt_pman_fault[] = "%s -> fatal error: program tried to fight with PMAN and PMAN won.";
char exmsg[512];

struct console_proc_info *get_console_process_info(int console)
{
	CPOSITION it = get_head_position(&running);

	while(it != NULL)
	{
		struct console_proc_info *pinf = (struct console_proc_info *)get_next(&it);
		if(pinf->console == console) return pinf;
	}
	return NULL;
}

struct console_proc_info *get_process_info(int task)
{
	CPOSITION it = get_head_position(&running);

	while(it != NULL)
	{
		struct console_proc_info *pinf = (struct console_proc_info *)get_next(&it);
		if(pinf->task == task) return pinf;
	}
	return NULL;
}

struct console_proc_info *get_process_info_byid(int id)
{
	CPOSITION it = get_head_position(&running);

	while(it != NULL)
	{
		struct console_proc_info *pinf = (struct console_proc_info *)get_next(&it);
		if(pinf->id == id) return pinf;
	}
	return NULL;
}

struct console_proc_info *process_ack(int task)
{
	struct console_proc_info *pinf = get_process_info(task);

	if(pinf == NULL) return NULL;

    if(pinf->flags & PINF_FLAG_INITIALIZED)
    {
	    if(pinf->param_smo != -1) claim_mem(pinf->param_smo); 

	    pinf->param_smo = -1;

	    if(pinf->map_smo != -1)
	    {
		    claim_mem(pinf->map_smo); 
		    pinf->map_smo = -1;
	    }
    }
    else
    {
        struct stdprocess_init initmsg;
        
        // send the init message
        pinf->flags |= PINF_FLAG_INITIALIZED;
        
        // send process initialization message
	    initmsg.command = STDPROCESS_INIT;
	    initmsg.shell_task = get_current_task();
	    initmsg.consoleid = pinf->console;
	    initmsg.cl_smo = pinf->param_smo;
	    initmsg.ret_port = SHELL_INITRET_PORT;

	    if(pinf->maps.mapstdout || pinf->maps.mapstdin || pinf->maps.mapstderr)
	        initmsg.map_smo = pinf->map_smo;
	    else
	        initmsg.map_smo = -1;
	    
        send_msg(pinf->task, STDPROCESS_PORT, &initmsg);
    }

	return pinf;
}

void task_finished(int task, int code)
{
	// close smos if still open
	struct console_proc_info *pinf = process_ack(task);
	struct console_proc_info *ptinf = NULL;
	int piped_finished = 0;

	if(pinf == NULL) return;

	// see if the task has a console and get it back if so
	if(pinf->console != -1)
	{
		// get console back
		get_console(pinf->console);

		cslown[pinf->console].mode = SHELL_CSLMODE_SHELL; // console is taken by the shell again
		cslown[pinf->console].process = -1;

		show_exception(pinf->console, code, pinf);
	}

	// close std redirections
	if(pinf->piped_to_task != -1)
	{
		// see if piped task exists
		ptinf = get_process_info_byid(pinf->piped_to_task);

		if(ptinf == NULL)
		{
			piped_finished = 1;

			if(pinf->flags & PINF_FLAG_STDIN_PIPED)
			{
				if(pinf->stdin != NULL) fclose(pinf->stdin);
			}
			else
			{
				if(pinf->stdout != NULL) fclose(pinf->stdout);
			}
		}
		else
		{
			piped_finished = 0;
			
			if(!(pinf->flags & PINF_FLAG_STDIN_PIPED))
			{
				if(pinf->stdin != NULL) fclose(pinf->stdin);
			}
			else
			{
				if(pinf->stdout != NULL) fclose(pinf->stdout);
			}
		}
	}
	else
	{
		piped_finished = 1;

		if(pinf->stdin != NULL) fclose(pinf->stdin);
		if(pinf->stdout != NULL) fclose(pinf->stdout);
	}
	
	if(pinf->stderr != NULL && pinf->stderr != pinf->stdout)
	{
		fclose(pinf->stderr);
	}

	// remove task from list
	CPOSITION it = get_head_position(&running);
	CPOSITION cit = NULL;

	while(it != NULL)
	{
		cit = it;
		ptinf = (struct console_proc_info *)get_next(&it);
		if(ptinf == pinf)
		{
			remove_at(&running, cit);
			break;
		}
	}
	
	free(pinf->cmd_name);

	int term = pinf->running_term;
	int console = pinf->console;

	free(pinf);

	// continue batched execution
	if(term != -1 && cslown[term].batched_index != -1 && piped_finished && code == 0)
	{
		if(run_command(term, csl_cmd[term])) return;
	}
	
	// show prompt
	if(console != -1)
	{
		term_print(console, "\n");
		show_prompt(console);		
	}
	
}

int destroy_tsk(int task)
{
	struct pm_msg_destroy_task msg;
	struct pm_msg_response msg_res;
	int sender_id;

	msg.pm_type = PM_DESTROY_TASK;
  	msg.req_id = task;
	msg.ret_value = -1;
	msg.task_id = task;
	msg.response_port = PM_TASK_ACK_PORT;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

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
			    task_finished(((struct pm_msg_finished*)&msg_res)->taskid, ((struct pm_msg_finished*)&msg_res)->ret_value);
				
			   	reschedule();
			}
			else if(msg_res.pm_type == PM_DESTROY_TASK)
			{
				break;
			}
		}
		else
		{
			reschedule();
		}
	}

	if(msg_res.status != PM_OK)
	{
		return 0;
	}

	return 1;
}

void show_exception(int term, int code, struct console_proc_info *pinf)
{
	char *msg;

	if(code >= -1 || code < -12) return;

	switch(code) {
		case DIV_ZERO_RVAL:
			msg = txt_div_zero;			
			break;
		case OVERFLOW_RVAL:
			msg = txt_ovflw;
			break;
		case BOUND_RVAL:
			msg = txt_bound;
			break;
		case INV_OPC_RVAL:
			msg = txt_inv_opcode;
			break;
		case DEV_NOT_AV_RVAL:
			msg = txt_dev_not_avl;
			break;
		case STACK_FAULT_RVAL:
			msg = txt_stack_fault;
			break;
		case GEN_PROT_RVAL:
			msg = txt_gpf;
			break;
		case PAGE_FAULT_RVAL:
			msg = txt_pgf;
			break;
		case FP_ERROR_RVAL:
			msg = txt_fp_err;
			break;
		case ALIG_CHK_RVAL:
			msg = txt_alig_chk;
			break;
		case SIMD_FAULT_RVAL:
			msg = txt_simd_fault;
			break;
        case PG_IO_ERROR:
			msg = txt_pgio_fault;
			break;
        case PG_RW_ERROR:
			msg = txt_pgrw_fault;
			break;
        case MAXADDR_ERROR:
			msg = txt_maxaddr_fault;
			break;
        case FMAP_IO_ERROR:
			msg = txt_fmapio_fault;
			break;
        case NOT_ENOUGH_MEMORY:
			msg = txt_mem_fault;
			break;
        case PMAN_RULEZ:
			msg = txt_pman_fault;
			break;
	}

	sprintf(exmsg, msg, pinf->cmd_name);
	term_color_print(term, exmsg, 12);
	
}
