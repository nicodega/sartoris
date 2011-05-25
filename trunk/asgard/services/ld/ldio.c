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


#include "ldio.h"

int g_ioport = IOPORT;

#define ERRSOURCE_DIR 0
#define ERRSOURCE_FS  1

int fopen(char *filename, FILE *file)
{
	// get filesystem for the file 
	struct stdfss_open open_msg;
	struct stdfss_open_res open_res;
	int intmode = 0, flags = 0;
	
	// resolve default fs service //
	file->fs_serviceid = resolve_fs();

	if(file->fs_serviceid == -1)
	{
		return 0;
	}
	//////////////////////

    file->eof = 0;

	intmode |= STDFSS_FILEMODE_READ;
	flags |= STDFSS_FILEMODE_MUSTEXIST;
	
	open_msg.command = STDFSS_OPEN;
	open_msg.file_path = share_mem(file->fs_serviceid, filename, len(filename)+1, READ_PERM);
	open_msg.open_mode = intmode;
	open_msg.ret_port = g_ioport;
	open_msg.flags = flags;

	send_fs(file->fs_serviceid, (int*)&open_msg, (int*)&open_res);

	claim_mem(open_msg.file_path);

	if(open_res.ret != STDFSSERR_OK)
		return 0;
	
	file->file_id = open_res.file_id;

	return 1;
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

	if(res.ret != STDFSSERR_OK)
		return 1;
		
	return 0;
};

size_t fread(char *buffer, int buffer_size, FILE *stream)
{
	struct stdfss_read read_cmd;
	struct stdfss_res res;

	if(stream->eof) return 0;

	if(stream == NULL) return 0;

	// close the file 
	read_cmd.command = STDFSS_READ;
	read_cmd.file_id = stream->file_id;
	read_cmd.count = buffer_size;
	read_cmd.smo = share_mem(stream->fs_serviceid, buffer, buffer_size, WRITE_PERM);
	read_cmd.ret_port = g_ioport;
    
	send_fs(stream->fs_serviceid, (int*)&read_cmd, (int*)&res);

	claim_mem(read_cmd.smo);

	stream->eof = res.param2;
    	
	return res.param1;
}

int fseek (FILE * stream, long offset)
{
	struct stdfss_seek seek_cmd;
	struct stdfss_res res;

	if(stream == NULL) return 1;

	// seek on the file 
	seek_cmd.command = STDFSS_SEEK;
	seek_cmd.file_id = stream->file_id;
	seek_cmd.origin = STDFSS_SEEK_SET;
	seek_cmd.possition = offset;
	seek_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&seek_cmd, (int*)&res);

	stream->eof = res.param2;

	if(res.ret != STDFSSERR_OK)
		return 1;
	
	return 0;
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
		return -1;
	}

	return dir_res.ret_value;
}

int get_response(int task, int *res)
{
	int id, dir = (task == DIRECTORY_TASK);
	int currthr = get_current_thread();
    
	for(;;)
	{
		while (get_msg_count(g_ioport) == 0) { reschedule(); }

        get_msg(g_ioport, res, &id);

		if(dir && id == task && currthr == ((struct directory_response *)res)->thr_id)
			return 1;
		else if(!dir && id == task && currthr == ((struct stdfss_res *)res)->thr_id)
			return 1;
	}
}

