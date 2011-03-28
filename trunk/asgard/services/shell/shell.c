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

/*****************************/

// container for environment variables
list vars[NUM_TERMS + 1];

list running;

char txt_init[] = "\nAsgard release 0.2\nMicrokernel version 2.0 alpha\n";
char *txt_csl[] = { 
 "Console 0 ready.",
  "Console 1 ready.",
  "Console 2 ready.",
  "Console 3 ready.",
  "Console 4 ready.",
  "Console 5 ready.",
  "Console 6 ready.",
  "Console 7 ready."
};

// "user@machine:dir# "
char machine_name[] = MACHINE_NAME;
char user_name[] = "root";

char csl_dir[NUM_TERMS][256];
char csl_cmd[NUM_TERMS][MAX_COMMAND_LINE_LEN];
int csl_smo[NUM_TERMS];

char str_buffer[BUFFER_SIZE];
char out_buffer[BUFFER_SIZE];
char in_buffer[BUFFER_SIZE];
char malloc_buffer[1024 * 30]; //  30 KB

int console_task;

/* Command history */
char csl_lastcmd[NUM_TERMS][MAX_COMMAND_LINE_LEN];
char csl_lastcmdcnt[NUM_TERMS];

/* Console ports */
int console_stddev_port;
int console_stdchardev_port;

// terminal ownership flag
struct csl_ownership cslown[NUM_TERMS];

char *tty_service_name = "devices/tty";

/* Get consolte task id */
void get_console_task()
{	
	struct directory_resolveid resolve_cmd;
	struct directory_response dir_res;
	int sender_id;

	// resolve console service //
	resolve_cmd.command = DIRECTORY_RESOLVEID;
	resolve_cmd.ret_port = 1;
	resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, tty_service_name, 12, READ_PERM);
	resolve_cmd.thr_id = get_current_thread();
    
    while(send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &resolve_cmd) < 0)
    { 
        reschedule(); 
    }

	while (get_msg_count(1) == 0){ reschedule(); }
    
	get_msg(1, &dir_res, &sender_id);

	claim_mem(resolve_cmd.service_name_smo);

	if(dir_res.ret != DIRECTORYERR_OK)
    {
		console_task = -1;
    }
	else
    {
		console_task = dir_res.ret_value;	
    }
}

/* entry point, interruptions are disabled */
void service_main (void)
{
	int i, id, term;
	struct stdchardev_res csl_res;
	struct csl_key_signal signal;
	struct stdchardev_seek_cmd seek0;
	struct pm_msg_response pm_res;
	struct stdprocess_res proc_res;

	seek0.command = CHAR_STDDEV_SEEK;
	seek0.flags = CHAR_STDDEV_SEEK_SET;
	seek0.pos = 0;
	seek0.ret_port = CSL_SEEK_PORT;
        
    // open ports
	open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(SHELL_PORT, -1, UNRESTRICTED);
    open_port(CSL_SCAN_ACK_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_ACK_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_SIGNAL_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_SEEK_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_REQUEST_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(PM_TASK_ACK_PORT, 1, PRIV_LEVEL_ONLY);
    open_port(PM_THREAD_ACK_PORT, 1, PRIV_LEVEL_ONLY);
    open_port(SHELL_INITRET_PORT, -1, UNRESTRICTED);
    open_port(IOLIB_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(DIRLIB_PORT, 2, PRIV_LEVEL_ONLY);
	
	_sti;
	
	init_mem(malloc_buffer, 1024 * 30);  
     
    /* Get console task from directory */
    i = 7;
	do
	{
		get_console_task();
	}
	while(console_task == -1);
	  
    set_ioports(IOLIB_PORT);
	initio();

	init_environment();
   
	init_consoles();
    init(&running);
    
    int k = 0;
	/* Message loop */
	for(;;)
	{   
        string_print("SHELL ALIVE",22*160,k++);

		/* process console read responses */
		while (get_msg_count(CSL_SCAN_ACK_PORT)>0) 
		{
			get_msg(CSL_SCAN_ACK_PORT, &csl_res, &id);
            if (id == CONS_TASK && csl_res.ret == STDCHARDEV_OK) 
			{
                i = csl_res.msg_id;
				if (0 <= i && i < NUM_TERMS && cslown[i].mode == SHELL_CSLMODE_SHELL) 
				{
            		if(csl_cmd[i][0] && csl_cmd[i][0] != '\n')
					{
            			csl_cmd[i][MAX_COMMAND_LINE_LEN - 1] = '\0';

						if(strreplace(csl_cmd[i], 0, '\n', '\0'))
	    				{
							strcp(csl_lastcmd[i], csl_cmd[i]);

							csl_lastcmdcnt[i] = 1;

							run_command(i, csl_cmd[i]);
	    				}
	    				else
	    				{
							term_print(i, "\nCommand is too long.\n");
							show_prompt(i);
	        			}
	  				}
					else
					{
                        term_print(i, "\nInvalid command.\n");
						show_prompt(i);
	  				}
				}
				else
				{
					__asm__ __volatile__ ("outb %1, %0" : : "dN" (0xe9), "a" (102));
					for(;;);
				}
			}
		}

		/* get signals */
		while (get_msg_count(CSL_SIGNAL_PORT)>0) 
		{
			get_msg(CSL_SIGNAL_PORT, &signal, &id);

			if (id == CONS_TASK) 
			{
				switch(signal.key & 0xFF)
				{
					case 'c':
						if(signal.mask & 0x3c) // Check for Ctrl+c
						{
							// ctrl + C was pressed, if there is a running process 
							// which has taken the console, finish it
							struct console_proc_info *pinf = get_console_process_info(signal.console);
							if(pinf != NULL)
							{
								// tell the process manager to destroy the task
								destroy_tsk(pinf->task);
							}
						}
						break;
					case 0x90:
						if(cslown[signal.console].mode != SHELL_CSLMODE_SHELL) break;

						if(!csl_lastcmdcnt[signal.console]) break;

						// seek to 0
						seek0.msg_id = 150 + signal.console;
						seek0.dev = signal.console;

						send_msg(CONS_TASK, console_stdchardev_port, &seek0);

						// wait for response
						while (get_msg_count(CSL_SEEK_PORT)==0){ reschedule();}
		
						get_msg(CSL_SEEK_PORT, &csl_res, &id);
								
						if(csl_res.ret == STDCHARDEV_OK)
						{
							// write old command to the console
							term_print(signal.console, csl_lastcmd[signal.console]);
							csl_lastcmdcnt[signal.console] = 0;
						}
						// if failed do nothing
					
						break;
					case 0x91:
						break;
				}
			}
		}

		process_env_msg();

		/* Get finished messages from process manager */

		while (get_msg_count(PM_TASK_ACK_PORT) > 0)
		{
		  	get_msg(PM_TASK_ACK_PORT, &pm_res, &id);

			if(id == PMAN_TASK && pm_res.pm_type == PM_TASK_FINISHED)
			{
				task_finished(((struct pm_msg_finished*)&pm_res)->taskid, ((struct pm_msg_finished*)&pm_res)->ret_value);
			}
		}

		/* Process standard process response asynchronously */
		while (get_msg_count(SHELL_INITRET_PORT) > 0)
		{
		  	get_msg(SHELL_INITRET_PORT, &proc_res, &id);

			process_ack(id);
		}
	}
}




/* 
 * Execute a shell command
 */
int run_command(int term, char* cmd)
{ 
  // support for ; separators (batching)
  char *cmd_line = cmd /*csl_cmd[term]*/ + ((cslown[term].batched_index != -1)? cslown[term].batched_index : 0);

  if(streq(cmd_line, '\0'))
  {
        show_prompt(term);
        return 0;
  }

  int i = 0, ln = len(cmd_line), brk = 0;
  while(i < ln) 
  {
	if(cmd_line[i] == '"')
	{
		brk = !brk;
	}
	else if(cmd_line[i] == ';' && !brk)
	{
		cmd_line[i] = '\0';
		if(cslown[term].batched_index != -1)
			cslown[term].batched_index += i + 1;
		else
			cslown[term].batched_index = i + 1;
		break;
	}
	i++;
  }

  if(i == ln)
  {
	cslown[term].batched_index = -1; // no more commands
  }

  trim(cmd_line);

  if(streq(cmd_line, '\0'))
  {
      show_prompt(term);
	  return 0;
  }

  term_print(term, "\n");

  /* See if it's an internal command */
  if(run_internal(term, cmd_line))
  {
	if(cslown[term].batched_index == -1)
	{
		show_prompt(term);
		return 1;
	}

	return run_command(term, cmd /*cmd[term]*/);
  }

  // not an internal command, attempt running
  if(run(term, cmd_line)) return 1;
	
  // ok, it's not a recognised command, fail
  term_color_print(term, "Unrecognised command.\n", 7);
  
  show_prompt(term);

  return 0;
}

