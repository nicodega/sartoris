/*
*
*	IOLIB 0.1: An implementation of an input/output library using the directory and 
*		   the ofs service implementation.
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


#include <lib/rawiolib.h>

int g_ioport = IOPORT;
int iolasterr = STDFSSERR_OK;

int map_error(int err, int source);

#ifdef IOLIB_MULTITHREADED
static list responses;
static int initialized = 0;
static struct mutex resmutex;
#endif

#define ERRSOURCE_DIR 0
#define ERRSOURCE_FS  1

void set_ioports(int ioport)
{
	g_ioport = ioport;
}

FILE *fopen(char *filename,char *mode)
{
	// get filesystem for the file 
	struct stdfss_open open_msg;
	struct stdfss_open_res open_res;
	FILE *file = (FILE *)malloc(sizeof(FILE));
	int intmode = 0, flags = 0;
	
	file->eof = 0;
	file->last_error = STDFSSERR_OK;

	// resolve default fs service //
	file->fs_serviceid = resolve_fs();

	if(file->fs_serviceid == -1)
	{
		free(file);
		return NULL;
	}
	//////////////////////

	// calculate opening mode
	if(streq(mode, "w") || streq(mode, "wb") || streq(mode, "wt"))
	{
		intmode |= STDFSS_FILEMODE_WRITE;
	}
	else if(streq(mode, "r") || streq(mode, "rb") || streq(mode, "rt"))
	{
		intmode |= STDFSS_FILEMODE_READ;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "a")  || streq(mode, "ab") || streq(mode, "at"))
	{
		intmode |= STDFSS_FILEMODE_APPEND;
	}
	else if(streq(mode, "r+") || streq(mode, "rb+") || streq(mode, "rt+") || streq(mode, "r+b") || streq(mode, "r+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_WRITE;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "w+") || streq(mode, "wb+") || streq(mode, "wt+") || streq(mode, "w+b") || streq(mode, "w+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_WRITE;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "a+") || streq(mode, "ab+") || streq(mode, "at+") || streq(mode, "a+b") || streq(mode, "a+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_APPEND;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else
	{
		iolasterr = IOLIBERR_INVALID_MODE;
		return NULL;	
	}
	
	open_msg.command = STDFSS_OPEN;
	open_msg.file_path = share_mem(file->fs_serviceid, filename, len(filename)+1, READ_PERM);
	open_msg.open_mode = intmode;
	open_msg.ret_port = g_ioport;
	open_msg.flags = flags;
#ifdef BUFFERED
	open_msg.flags  |= STDFSS_OPEN_WRITEBUFFERED;
#endif

	send_fs(file->fs_serviceid, (int*)&open_msg, (int*)&open_res);

	claim_mem(open_msg.file_path);

	if(open_res.ret != STDFSSERR_OK)
	{
		iolasterr = map_error(open_res.ret, ERRSOURCE_FS);
		free(file);
		return NULL;
	}

	file->file_id = open_res.file_id;

	return file;
	
}
int fclose(FILE *stream)
{
	struct stdfss_close close_msg;
	struct stdfss_res res;

	if(stream == NULL) return 1;

	// close the file 
	close_msg.command = STDFSS_CLOSE;
	close_msg.file_id = stream->file_id;
	close_msg.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&close_msg, (int*)&res);

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	free(stream);
	
	return 0;
}; // ret 0 = ok

size_t fwrite(int buffer, size_t size, size_t count, FILE *stream)
{
	int buffer_size = size * count;
	struct stdfss_write write_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 0;

	write_cmd.command = STDFSS_WRITE;
	write_cmd.file_id = stream->file_id;
	write_cmd.count = buffer_size;
	write_cmd.smo = buffer;
	write_cmd.ret_port = g_ioport;

	pass_mem(buffer, stream->fs_serviceid);

	send_fs(stream->fs_serviceid, (int*)&write_cmd, (int *)&res);

	claim_mem(write_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);
	
	if(res.ret != STDFSSERR_OK)
	{		
		return res.param1;
	}

	return res.param1;
}
size_t fread(int buffer, size_t size, size_t count,FILE *stream)
{
	int buffer_size = size * count;
	struct stdfss_read read_cmd;
	struct stdfss_res res;

	if(stream->eof) return 0;

	if(stream == NULL) return 0;

	// close the file 
	read_cmd.command = STDFSS_READ;
	read_cmd.file_id = stream->file_id;
	read_cmd.count = buffer_size;
	read_cmd.smo = buffer;
	read_cmd.ret_port = g_ioport;

	pass_mem(buffer, stream->fs_serviceid);

	send_fs(stream->fs_serviceid, (int*)&read_cmd, (int*)&res);

	claim_mem(read_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return res.param1;
	}

	return res.param1;
}

int fgets(int str, int len, FILE *stream)
{
	struct stdfss_gets gets_cmd;
	struct stdfss_res res;

	if(stream->eof) return 0;

	if(stream == NULL) return 0;

	// close the file 
	gets_cmd.command = STDFSS_GETS;
	gets_cmd.file_id = stream->file_id;
	gets_cmd.count = len;
	gets_cmd.smo = str;
	gets_cmd.ret_port = g_ioport;

	pass_mem(str, stream->fs_serviceid);

	send_fs(stream->fs_serviceid, (int*)&gets_cmd, (int*)&res);

	claim_mem(gets_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return NULL;
	}

	return str;
}
int fgetc(FILE *stream)
{
	struct stdfss_getc getc_cmd;
	struct stdfss_res res;

	if(stream->eof) return 0;

	if(stream == NULL) return 0;

	// close the file 
	getc_cmd.command = STDFSS_GETC;
	getc_cmd.file_id = stream->file_id;
	getc_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&getc_cmd, (int*)&res);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return EOF;
	}

	return ((struct stdfss_getc_res*)&res)->c;
}
int fputc(int c, FILE *stream)
{
	struct stdfss_putc putc_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 0;

	putc_cmd.command = STDFSS_PUTC;
	putc_cmd.file_id = stream->file_id;
	putc_cmd.c = c;
	putc_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&putc_cmd, (int *)&res);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);
	
	if(res.ret != STDFSSERR_OK)
	{	
		return EOF;
	}

	return c;
}
int fputs(int str, FILE *stream)
{
	struct stdfss_puts puts_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 0;

	puts_cmd.command = STDFSS_PUTS;
	puts_cmd.file_id = stream->file_id;
	puts_cmd.count = mem_size(str);
	puts_cmd.smo = str;
	puts_cmd.ret_port = g_ioport;

	pass_mem(str, stream->fs_serviceid);

	send_fs(stream->fs_serviceid, (int*)&puts_cmd, (int *)&res);

	claim_mem(puts_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);
	
	if(res.ret != STDFSSERR_OK)
	{
		
		return EOF;
	}

	return 1;
}

int ioerror(){return iolasterr;}
int ferror(FILE *stream){return stream->last_error;}

int  fseek (FILE * stream , long offset , int origin )
{
	struct stdfss_seek seek_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 1;

	// seek on the file 
	seek_cmd.command = STDFSS_SEEK;
	seek_cmd.file_id = stream->file_id;
	seek_cmd.origin = origin;
	seek_cmd.possition = offset;
	seek_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&seek_cmd, (int*)&res);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

long ftell(FILE *stream)
{
	struct stdfss_tell tell_cmd;
	struct stdfss_tell_res res;

	if(stream == NULL) return -1;

	// tell
	tell_cmd.command = STDFSS_TELL;
	tell_cmd.file_id = stream->file_id;
	tell_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&tell_cmd, (int*)&res);

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return -1;
	}

	return (long)res.cursor;
}

int fflush(FILE *stream)
{
	struct stdfss_flush flush_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 1;

	// flush
	flush_cmd.command = STDFSS_FLUSH;
	flush_cmd.file_id = stream->file_id;
	flush_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&flush_cmd, (int*)&res);

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

int feof(FILE *stream)
{
	return stream->eof;
}

int map_error(int err, int source)
{
	if(source == ERRSOURCE_DIR)	
	{
		// directory error
		return err;
	}
	else
	{
		if(err == 0) return 0;
		// stdfss error
		return err + 2;
	}
}

char *mapioerr(int ret)
{
	switch(ret)
	{
		case IOLIBERR_OK:
			return "OK";
		case IOLIBERR_INVALID_MODE:
			return "Invalid Mode";
		case IOLIBERR_DIR_NOTREGISTERED:
			return "Not Registered";
		case IOLIBERR_DEVICE_NOT_MOUNTED:
			return "Device not mounted";
		case IOLIBERR_PATH_ALREADY_MOUNTED:
			return "Pah already mounted";
		case IOLIBERR_DEVICE_ALREADY_MOUNTED:
			return "Device already mounted";
		case IOLIBERR_DEVICE_FILE_NOT_FOUND:
			return "File not found";
		case IOLIBERR_SERVICE_NOT_INITIALIZED:
			return "Service not initialized";
		case IOLIBERR_NOT_ENOUGH_MEM:
			return "Not enough memory";
		case IOLIBERR_COULD_NOT_ACCESS_DEVICE:
			return "Could not access device";
		case IOLIBERR_STDFS_SERVICE_SHUTDOWN:
			return "Service is shutting down";
		case IOLIBERR_SERVICE_ALREADY_INITIALIZED:
			return "Service already initialized";
		case IOLIBERR_INTERFACE_NOT_FOUND:
			return "Interface not found";
		case IOLIBERR_DEVICE_ERROR:
			return "Device Error";
		case IOLIBERR_FORMAT_NOT_SUPPORTED:
			return "Format not supported";
		case IOLIBERR_INVALID_COMMAND:	
			return "Invalid command";
		case IOLIBERR_INVALID_COMMAND_PARAMS:
			return "Invalid command parameters";
		case IOLIBERR_EOF:
			return "End of File reached";
		case IOLIBERR_FATAL:
			return "Fatal error";
		case IOLIBERR_NOT_FOUND:
			return "Not found";
		case IOLIBERR_FILE_INUSE:
			return "File is being used";
		case IOLIBERR_FILE_DOESNOTEXIST:
			return "File does not exists";
		case IOLIBERR_FILE_INVALIDOPERATION:
			return "File Invalid operation";
		case IOLIBERR_FILE_DIRNOTEMPTY:
			return "Directory is not empty";
		case IOLIBERR_FILE_NOTOPEN:
			return "File is not open";
		case IOLIBERR_TOOMANY_FILESOPEN:
			return "Too many files open";
		case IOLIBERR_DEVICE_MOUNTED:
			return "Device is mounted";
		case IOLIBERR_PATH_INUSE:
			return "Patyh is in use";
		case IOLIBERR_BAD_DEVICE_FILE:
			return "Bad device file";
		case IOLIBERR_DEVICE_INUSE:
			return "Device is being used";
		case IOLIBERR_FILE_EXISTS:
			return "File already exists";
		case IOLIBERR_DEVICE_NOTSUPPORTED:
			return "Device type is not supported";
		case IOLIBERR_NOT_DEVICEFILE:
			return "Not a device file";
		case IOLIBERR_NOT_SUPPORTED:
			return "Not supported";
		case IOLIBERR_NO_PRIVILEGES:
			return "No privileges";
		case IOLIBERR_SMO_ERROR:
			return "SMO error";
		case IOLIBERR_FS_ERROR:
			return "File system has invalid data. disk may be corrupted.";
		case IOLIBERR_FILENAME_TOOLONG:
			return "File name is too long.";
		case IOLIBERR_INVALID_PATH:
			return "Invalid path.";
		default:
			return "Unknown error";
	}
}

int send_fs(int task, int *msg, int *res)
{

	((struct stdfss_cmd *)msg)->thr_id = get_current_thread();

	send_msg(task, STDFSS_PORT, msg);

	return get_response(task, res);	
}

int resolve_fs()
{
	char *service_name = "fs/default";
	struct directory_resolveid resolve_cmd;
	struct directory_response dir_res;

	// resolve default fs service //
	resolve_cmd.command = DIRECTORY_RESOLVEID;
	resolve_cmd.ret_port = g_ioport;
	resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name)+1, READ_PERM);
	resolve_cmd.thr_id = get_current_thread();

	send_msg(DIRECTORY_TASK,DIRECTORY_PORT, &resolve_cmd);

	get_response(DIRECTORY_TASK, (int*)&dir_res);

	claim_mem(resolve_cmd.service_name_smo);

	if(dir_res.ret != DIRECTORYERR_OK)
	{
		iolasterr = map_error(dir_res.ret, ERRSOURCE_DIR);
		return -1;
	}

	return dir_res.ret_value;
}

void initio()
{
#ifdef IOLIB_MULTITHREADED
	if(!initialized)
	{
		init_mutex(&resmutex);
		init(&responses);
		initialized = 1;
	}
#endif
}

int get_response(int task, int *res)
{
	int id, dir = (task == DIRECTORY_TASK);
	int currthr = get_current_thread();

#ifdef IOLIB_MULTITHREADED
	struct response *resentry = NULL, *ent = NULL;
	CPOSITION entry = NULL, it = NULL;

	// create an entry on the waiting list
	resentry = (struct response *)malloc(sizeof(struct response));

	resentry->thr = currthr;
	resentry->task = task;
	resentry->received = 0;

	wait_mutex(&resmutex);
	add_head(&responses, resentry);
	entry = get_head_position(&responses);
	leave_mutex(&resmutex);
#endif

	for(;;)
	{
#ifdef IOLIB_MULTITHREADED
		// look if there is a response on the io port
		while (get_msg_count(g_ioport) == 0 && !resentry->received) { reschedule(); }

		if(resentry->received)
		{
			wait_mutex(&resmutex);
			// remove our entry
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
			return 1;
		}
#else
		while (get_msg_count(g_ioport) == 0) { reschedule(); }
#endif
		get_msg(g_ioport, res, &id);

		if(dir && id == task && currthr == ((struct directory_response *)res)->thr_id)
		{
#ifdef IOLIB_MULTITHREADED
			wait_mutex(&resmutex);	
			// remove our entry
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
			
#endif
			return 1;
		}
		else if(!dir && id == task && currthr == ((struct stdfss_res *)res)->thr_id)
		{
#ifdef IOLIB_MULTITHREADED
			// remove our entry
			wait_mutex(&resmutex);
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
#endif
			return 1;
		}
#ifdef IOLIB_MULTITHREADED
		else
		{
			wait_mutex(&resmutex);
			// message was not ment for us, see if it belongs to other thread
			it = get_head_position(&responses);

			while(it != NULL)
			{
				ent = (struct response *)get_next(&it);

				if( ent->task == id && !ent->received && ((ent->task == DIRECTORY_TASK && ent->thr == ((struct directory_response *)res)->thr_id) || (ent->task != DIRECTORY_TASK && ent->thr == ((struct stdfss_res *)res)->thr_id)))
				{
					// signal
					ent->received = 1;
					break;
				}
			}
			leave_mutex(&resmutex);
		}
#endif

	}
}	

