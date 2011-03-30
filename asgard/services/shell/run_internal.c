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

// parameters per terminal
static char **args[NUM_TERMS];
int argc[NUM_TERMS];

void free_params(int term);

int run_internal(int term, char* cmd_line)
{
	char cmd[MAX_COMMAND_LEN];
	char params[MAX_COMMAND_LEN];
	int index;

	memset(cmd, '\0', MAX_COMMAND_LEN);
	memset(params, '\0', MAX_COMMAND_LEN);

	if(!strword(1, cmd_line, len(cmd_line)+1, cmd, MAX_COMMAND_LEN)) 
	{
		return 0;
	}

	index = first_index_of(cmd_line, ' ', len(cmd));

	if(index != -1)
		substr(cmd_line, index, -1, params);
	else
		params[0] = '\0';

	trim(params);

	argc[term] = get_param_count(params);
	if(argc[term] > 0)
	{
		args[term] = (char**)malloc(argc[term] * sizeof(char*));
		get_parameters(params, argc[term], args[term]);
	}
	else
	{
		args[term] = NULL;
	}

	if(streq(cmd, "cd"))
	{
		if(streq(params, ""))
  		{
			term_color_print(term, "Wrong parameters.\n\n", 7);
			term_color_print(term, "Usage: cd path.\n\n", 7);
		}
		else
		{
			change_dir(term, params);
		}
		free_params(term);

		return TRUE;
	}
	else if(streq(cmd, "set"))
	{
		// allows setting, listing or removing global and console variables
		set_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "jobs"))
	{
		// allows setting, listing or removing global and console variables
		jobs(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "kill"))
	{
		// allows setting, listing or removing global and console variables
		kill(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "clear"))
	{
		term_clean(term);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "initofs")) // this command should dissapear in a future
	{
		init_ofs(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "mkdir")) 
	{
		mkdir_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "rm")) 
	{
		rm_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "ls")) 
	{
		ls(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "mkdevice")) 
	{
		mkdevice_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "ofsformat")) 
	{
		ofsformat(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "cat")) 
	{
		cat(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "write"))
	{
		write(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "mount"))
	{
		mount_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "umount"))
	{
		umount_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "chattr"))
	{
		chattr_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "ln"))
	{
		mklink_cmd(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	else if(streq(cmd, "contest"))
	{
		contest(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
    else if(streq(cmd, "atactst"))
	{
		atactst(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
    else if(streq(cmd, "dyntst"))
	{
		dyntst(term, args[term], argc[term]);
		free_params(term);
		return TRUE;
	}
	free_params(term);
	return FALSE;
}


void free_params(int term)
{
	if(argc[term] != 0)
	{
		free(args[term]);
		args[term] = NULL;
	}
}



