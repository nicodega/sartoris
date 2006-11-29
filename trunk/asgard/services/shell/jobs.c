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

void jobs(int term, char **args, int argc)
{
	CPOSITION it = NULL;
	struct console_proc_info *pinf = NULL;
	char buffer[BUFFER_SIZE];
	int i = 0;

	// show tasks	
	it = get_head_position(&running);

	term_color_print(term, "Running processes:\n", 7);

	while(it != NULL)
	{
		pinf = (struct console_proc_info *)get_next(&it);

		// ignore globals when locally defined
		if(pinf->console != -1)
		{
			sprintf(buffer, "id %i %s | tid %i | cons %d \n", i, pinf->cmd_name, pinf->task, pinf->console);
		}
		else
		{
			sprintf(buffer, "id %i %s | tid %i | cons %d | bg \n", i, pinf->cmd_name, pinf->task, pinf->running_term);
		}
		term_color_print(term, buffer, 11);
		i++;
	}
}

void kill(int term, char **args, int argc)
{
	CPOSITION it = NULL;
	struct console_proc_info *pinf = NULL;
	int task = -1;
	
	if(argc != 1 || (args[0][0] != '%' && !isnumeric(args[0])) || (args[0][0] == '%' && !isnumeric(&args[0][1])))
	{
		term_color_print(term, "Invalid parameters\nUsage: kill {%id | tid}\n", 12);
		return;
	}

	if(args[0][0] == '%')
	{
		int id = atoi(&args[0][1]);
		if(id < 0)
		{
			term_color_print(term, "Id must be greater or equal than zero.\n", 12);
			return;
		}
		it = get_head_position(&running);
		while(it != NULL && id >= 0)
		{
			pinf = (struct console_proc_info *)get_next(&it);
			if(id == -1) break;
			id--;
		}
		if(id != -1)
		{
			term_color_print(term, "Invalid Id.\n", 12);
			return;
		}
		task = pinf->task;
	}
	else
	{
		task = atoi(args[0]);
		pinf = get_process_info(task);	
		if(pinf == NULL)
		{
			term_color_print(term, "Invalid task id.\n", 12);
			return;
		}	
	}

	if(!destroy_tsk(task))
	{
		term_color_print(term, "Could not finish task.\n", 12);
		return;
	}
}

