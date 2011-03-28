/*
*
* Commands for testing ofs file system
*
*/

#include "shell_iternals.h"

static int initialized = 0;
char *invalid_params = "Invalid parameters.\n\0\0";

// initofs ["service name on directory"|device id] [logic device]
void init_ofs(int term, char **args, int argc)
{
	struct stdfss_init init_msg;	
	struct stdfss_res res;
	int id;
	int serviceid = 0;
	int logic_dev = 0;
	char path[4];
	struct directory_resolveid resolve_cmd;
	struct directory_response dir_res;
	int ofs_task = resolve_fs();	// function call from iolib
	
	if(!initialized)
	{
		if(argc == 0)
		{
			term_color_print(term, invalid_params, 12);
			term_color_print(term, "Usage: initofs <[Service name] | [Service id]> [logic device nr].\n", 7);
			return;
		}

		if(argc > 2 || (argc == 2 && !isnumeric(args[1])))
		{
			term_color_print(term, invalid_params, 12);
			term_color_print(term, "Usage: initofs <[Service name] | [Service id]> [logic device nr].\n", 7);
			return;
		}

		if(argc > 0 && !isnumeric(args[0]))
		{
			// resolve default fs service //
			resolve_cmd.command = DIRECTORY_RESOLVEID;
			resolve_cmd.ret_port = IOLIB_PORT;
			resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, args[0], len(args[0]) + 1, READ_PERM);
					
			send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &resolve_cmd);

			while (get_msg_count(IOLIB_PORT) == 0) { }

			get_msg(IOLIB_PORT, &dir_res, &id);

			claim_mem(resolve_cmd.service_name_smo);

			if(dir_res.ret != DIRECTORYERR_OK)
			{
				term_color_print(term, "\nInvalid service directory name.\n", 12);
				return;
			}
			term_color_print(term, "\nResolved service id.\n", 7);

			serviceid = dir_res.ret_value;
		}	
		else if(argc > 0)
		{
			serviceid = atoi(args[0]);
		}

		if(argc == 2)
		{
			logic_dev = atoi(args[1]);
		}

		term_color_print(term, "Initializing OFS Service... ", 7);

		path[0] = '/';
		path[1] = '\0';

	
		init_msg.command = STDFSS_INIT;
		init_msg.path_smo = share_mem(ofs_task, path, 2, READ_PERM);
		init_msg.ret_port = INITOFS_PORT;
		init_msg.deviceid = serviceid;
		init_msg.logic_deviceid = logic_dev;

		send_msg(ofs_task, STDFSS_PORT, &init_msg);

		while(get_msg_count(INITOFS_PORT) == 0){ reschedule(); } // wait for OFS response

		get_msg(INITOFS_PORT, &res, &id); 

		claim_mem(init_msg.path_smo);

		if(res.ret != STDFSSERR_OK)
		{
			term_color_print(term, " FAILED\n", 12);
			return;
		}

		term_color_print(term, " OK\n", 11);
	}
	else
	{
		term_color_print(term, "OFS Already initialized\n", 11);
	}
	
	initialized = 1;

}

void mkdir_cmd(int term, char **args, int argc)
{
	char *path = NULL;

	if(argc != 1)
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: mkdir dir_path.\n", 7);
		return;
	}
	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid directory.\n", 7);
		return;
	}	

	if(mkdir(path))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 12);
	}
	free(path);
}

void rm_cmd(int term, char **args, int argc)
{
	char *path = NULL;

	if(argc != 1)
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: rm filepath.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid directory.\n", 7);
		return;
	}

	if(rm(path))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 12);
	}
	free(path);
}

// creates a device file
// mkdevice filename service_name logic_device
void mkdevice_cmd(int term, char **args, int argc)
{
	int logic_dev = 0;
	char *path = NULL;

	// check parameters
	if(argc != 3 || !isnumeric(args[2]))
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: mkdevice filename Service_name logic_device.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
		return;
	}

	logic_dev = atoi(args[2]);

	if(mkdevice(path, args[1], logic_dev))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 12);
	}
	free(path);
}

// usage:
// ofsformat device_file
static int format_term;
void format_print(char *str, int color)
{
	term_color_print(format_term, str, color);
}

void ofsformat(int term, char **args, int argc)
{
	char *path = NULL;

	if(argc != 1)
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: ofsformat device_file.\n", 7);
		return;
	}
	
	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
		return;
	}

	term_color_print(term, "Formatting...\n", 7);

	format_term = term;

	if(format(path, FORMAT_SIMPLE_OFS_TYPE, NULL, 0, format_print))
	{
		term_color_print(term, "\nFAILED\n", 12);
	}
	term_color_print(term, "\n\nFormat finished\n", 7);

	free(path);
}

// shows the contents of a file
// on console
// usage: cat filename
void cat(int term, char **args, int argc)
{
	FILE *f;
	FILEINFO *finf;
	char buffer[132];
	int s;
	char *path = NULL;

	if(argc != 1)
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: cat filename.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
		return;
	}
		
	finf = finfo(path);

	if(finf == NULL)
	{
		free(path);
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	if(finf->file_type == IOLIB_FILETYPE_DIRECTORY)
	{
		free(path);
		term_color_print(term, "Cannot cat directories.\n", 12);
		return;
	}

	f = fopen(path, "r");

	if(f == NULL)
	{
		free(path);
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	while(finf->file_size > 0)
	{
		s = MIN(129, (unsigned int)finf->file_size);
		if(fread(buffer, 1, s, f) != s)
		{
			fclose(f);
			free(path);
			term_color_print(term, mapioerr(ioerror()), 12);
			term_color_print(term, "\n", 11);
			return;
		}
		buffer[s] = '\0';
		term_color_prints(term, buffer, 7, s, 0);
		finf->file_size -= s;
	}

	fclose(f);
	
	free(finf);
	free(path);

	term_color_print(term, "\n", 12);
}

// writes a string given as a parameter at 
// the specified possition
void write(int term, char **args, int argc)
{
	int possition = 0, str_pos = 2;
	FILE *f;
	char *path = NULL;

	if((argc != 3 && argc != 4) || (argc == 4 && !isnumeric(args[1])) )
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: write filename {a|w} [possition] string.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
		return;
	}

	if(argc == 4)
	{
		possition = atoi(args[2]);
		str_pos = 2;
	}

	if(argc == 3)
	{
		if(streq(args[1], "w")) 
			f = fopen(path, "w");
		else
			f = fopen(path, "a");
	}

	if(f == NULL)
	{
		free(path);
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	if(possition != 0 && fseek(f, possition, SEEK_SET))
	{
		free(path);
		fclose(f);
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	if(fwrite(args[str_pos],1, len(args[str_pos]), f) != len(args[str_pos]))
	{
		free(path);
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		fclose(f);
		return;
	}

	fclose(f);
	free(path);

	return;
}

// changes a given file attributes
void chattr_cmd(int term, char **args, int argc)
{
	int flags = 0, substract = 0;
	char *path = NULL;

	// changeable attributes are: hidden and readonly
	// -|+{h,r}
	if(argc != 2 || len(args[1]) == 1 || (args[1][0] != '+' && args[1][0] != '-'))
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: chattr filename +|-{h,r}.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
		return;
	}

	if(args[1][0] == '-')
	{
		substract = 1;
	}

	if(first_index_of(args[1], 'h', 0) != -1)
	{
		flags |= IOLIB_ATT_HIDDEN;
	}
	if(first_index_of(args[1], 'r', 0) != -1)
	{
		flags |= IOLIB_ATT_READONLY;
	}

	if(chattr(path, flags, substract))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
	}
	free(path);
}

// creates a hard link
void mklink_cmd(int term, char **args, int argc)
{
	char *path = NULL, *path2 = NULL;

	if(argc != 2)
	{
		term_color_print(term, invalid_params, 12);
		term_color_print(term, "Usage: ln filename linkname.\n", 7);
		return;
	}

	path = build_path(args[0], term);

	if(path == NULL)
	{
		term_color_print(term, "Invalid filename.\n", 7);
		return;
	}

	path2 = build_path(args[1], term);

	if(path2 == NULL)
	{
		term_color_print(term, "Invalid linkname.\n", 7);
		return;
	}

	if(mklink(path, path2))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
	}
	free(path);
	free(path2);
}

