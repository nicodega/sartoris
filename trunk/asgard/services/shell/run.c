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

char *build_file_path(char *cmd, int term);
char *get_cmd(char *cmd_line, int length);
int ispiped(char *cmd_line);
struct console_proc_info *run_partial(int term, char *cmd_line, int piped, int pipeis_stdin, int *console);

int run(int term, char *cmd_line)
{
	// pipes support
	int pipepos = ispiped(cmd_line);

	if(pipepos != -1)
	{
		char *cmd_line2 = cmd_line + pipepos;

		// start execution of first command
		int console = -1, console2 = term;

		trim(cmd_line2);
		trim(cmd_line);

		struct console_proc_info *pinf = run_partial(term, cmd_line, 1, 0, &console);
		if(pinf == NULL)
		{
			return FALSE;
		}

		struct console_proc_info *pinf2 = run_partial(term, cmd_line2, 1, 1, &console2);
		if(pinf2 == NULL)
		{
			destroy_tsk(pinf->task);
			task_finished(pinf->task, -1);
			return FALSE;
		}

		// FIXED: Now an internal task id is used for pipes (because of task id reuse)
		pinf->piped_to_task = pinf2->id;
		pinf2->piped_to_task = pinf->id;

		FILE *pipe = open_spipe(pinf->task, pinf2->task);

		if(pipe == NULL)
		{
			destroy_tsk(pinf->task);
			task_finished(pinf->task, -1);
			destroy_tsk(pinf2->task);
			task_finished(pinf2->task, -1);
			return FALSE;
		}

		if(!finish_execute(pinf, pipe, 0))
		{
			destroy_tsk(pinf2->task);
			task_finished(pinf2->task, -1);
		}

		if(!finish_execute(pinf2, pipe, 1))
		{
			// just close the pipe...
			fclose(pipe);
		}
		
		if(console2 == -1 && cslown[term].batched_index == -1)
			show_prompt(term); // show prompt, for command has not taken the console ^^

		return TRUE;
	}
	else
	{
		int console = term;
		struct console_proc_info *pinf = run_partial(term, cmd_line, 0, 0, &console);

		if(pinf == NULL || !finish_execute(pinf, NULL, 0))
		{
			return FALSE;
		}
		
		if(console == -1 && cslown[term].batched_index == -1)
			show_prompt(term); // show prompt, for command has not taken the console ^^

		return pinf != NULL;
	}

}

struct console_proc_info *run_partial(int term, char *cmd_line, int piped, int pipeis_stdin, int *console)
{
	char *path = NULL;
	char *cmd;

	cmd = get_cmd(cmd_line, len(cmd_line)+1);

	if(cmd == NULL) return NULL;

	path = build_file_path(cmd, term);

	if(path == NULL)
	{
		free(cmd);
		return NULL;
	}

	// remove path from command
	int index = last_index_of(cmd, '/');
	if(index != -1)
	{
		int j = index + 1, i = 0;
		while(cmd[j] != '\0')
		{
			cmd[i] = cmd[j];
			j++;i++;
		}
		cmd[i] = '\0';
	}

	// check for background process (&)
	if(last_index_of(cmd_line, '&') == len(cmd_line)-1 || *console == -1)
	{
		if(*console != -1) 
			cmd_line[len(cmd_line)-1] = '\0';

		*console = -1;
	}
	else
	{
		*console = term;
	}

	// execute
	struct console_proc_info *pinf = execute(term, *console, path, cmd, cmd_line, piped, pipeis_stdin);

	if(pinf == NULL)
	{
		free(cmd);
		cslown[term].batched_index == -1; // no more commands for command execution failed
	}
	
	free(path);
	return pinf;
}

int ispiped(char *cmd_line)
{
	int i = 0, ln = len(cmd_line), brk = 0;

	while(i < ln)
	{
		if(cmd_line[i] == '"')
		{
			brk = !brk;
		}
		else if(cmd_line[i] == '|' && !brk)
		{
			cmd_line[i] = '\0';
			return i + 1; // return start of second command
		}
		i++;
	}	

	if(i == ln)
	{
		return -1;
	}
}

char *get_cmd(char *cmd_line, int length)
{
	int start = 0;
	if(cmd_line[start] == '\0') return NULL;
	int end = start;
	
	while(cmd_line[end] != ' ' && cmd_line[end] != '\0' 
		&& !(cmd_line[end+1] == '>' && (cmd_line[end] == '>' || cmd_line[end] == '2' || cmd_line[end] == '1'))
		&& cmd_line[end] != '<' && !(cmd_line[end] == '0' && cmd_line[end] == '<')
		&& cmd_line[end] != '>'
	)
	{
		end++;
	}
	
	char *cmd = (char *)malloc((end - start) + 1);
	estrcopy(cmd_line + start, cmd, 0, (end - start));
	cmd[(end - start)] = '\0';

	return cmd;
}


/* This function will attempt to complete the file name, with a valid executable file. 
*  If function does not match any executable file, it'll return NULL.
*/
char *build_file_path(char *cmd, int term)
{
	char *path_var = NULL;
	char* current_dir = get_env("CURRENT_PATH", term);
	char *path = NULL, **paths = NULL;
	char command_path[MAX_COMMAND_LINE_LEN];
	int type, count, i = 0, ln;	

	// see if it's an absolute path
	if(cmd[0] == '.' || cmd[0] == '/') // is it a relative path?
	{
		path = build_path(cmd, term);

		type = filetype(path);

		if(type == IOLIB_FILETYPE_FILE)
		{
			return path;
		}

		free(path);

		return NULL;		
	}
	else
	{
		// ok it's just a name

		// see if it's on current term dir
		ln = len(current_dir);
		strcp(command_path, current_dir);

		if(!streq(current_dir, "/"))
		{
			command_path[ln] = '/';
			ln++;
		}

		estrcopy(cmd, command_path + ln + 1, 0, len(cmd) + 1);

		type = filetype(command_path);

		if(type == IOLIB_FILETYPE_FILE)
		{
			path = (char *)malloc(ln + len(cmd) + 1);
			strcp(path, command_path);
			return path;
		}

		// first try console env PATH
		path_var = get_env("PATH", term);

		if( path_var == NULL )
		{
			// try global env PATH
			path_var = get_global_env("PATH");

			if( path_var == NULL )
			{
				return FALSE;
			}
		}

		if(len(path_var) == 0) return FALSE;

		path = trim(strcopy(path_var));

		count = strsplit(0, ';', 1, path, path);

		paths = (char**)malloc(sizeof(char*) * count);

		// get_parameters will eat spaces left by strsplit
		get_parameters(path, count, paths);

		// FIXED: path was being freed while paths keeps pointers inside path

		while(i < count)
		{
			strcp(command_path, paths[i]);

			ln = len(command_path);
			if(command_path[ln] != '/')
			{
				command_path[ln] = '/';
				ln++;
			}

			estrcopy(cmd, command_path + ln, 0, len(cmd)+1);

			type = filetype(command_path);

			if(type == IOLIB_FILETYPE_FILE)
			{
				free(path);

				path = (char *)malloc(ln + len(cmd) + 1);
				strcp(path, command_path);
			
				free(paths);

				return path;
			}

			i++;
		}

		if(paths) free(paths);
		free(path);

		return NULL;	
	}
}


