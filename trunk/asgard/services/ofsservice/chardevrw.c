/*
*
*	OFS (Obsession File system) Multithreaded implementation
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
	
#include "ofs_internals.h"

struct stdfss_res *chardev_read(int wpid, struct working_thread *thread, struct stdfss_read *read_cmd, int delimited, struct stask_file_info *finf)
{
	struct stdfss_res *ret = NULL;
	struct stdchardev_read_cmd readcmd;
	struct stdchardev_readc_cmd readc;
	struct stdfss_res *res = NULL;
	int smo = read_cmd->smo;
	int *comm;
	char nullbuff = '\0';

	wait_mutex(&finf->dinf->blocked_mutex);
	
	if(finf->dinf->blocked)
	{
		leave_mutex(&finf->dinf->blocked_mutex);
		return build_response_msg(thread->command.command, STDFSSERR_DEVICE_INUSE);
	}

	switch(thread->command.command)
	{
		case STDFSS_GETS:
			// check device is not already blocked 
			readcmd.flags = CHAR_STDDEV_READFLAG_PASSMEM | CHAR_STDDEV_READFLAG_ECHO | CHAR_STDDEV_READFLAG_BLOCKING | CHAR_STDDEV_READFLAG_DELIMITED;
			pass_mem(read_cmd->smo, finf->deviceid);
			comm = (int*)&readcmd;
			readcmd.command = CHAR_STDDEV_READ;
			readcmd.dev = finf->logic_deviceid;
			readcmd.delimiter = '\n';
			readcmd.buffer_smo = smo;
			readcmd.byte_count = read_cmd->count;
			readcmd.msg_id = wpid;
			readcmd.ret_port = OFS_CHARDEV_PORT;
			break;
		case STDFSS_GETC:
			readc.flags = CHAR_STDDEV_READFLAG_ECHO | CHAR_STDDEV_READFLAG_BLOCKING;
			comm = (int*)&readc;
			readc.command = CHAR_STDDEV_READC;
			readc.dev = finf->logic_deviceid;
			readc.msg_id = wpid;
			readc.ret_port = OFS_CHARDEV_PORT;
			break;
		case STDFSS_READ:
			readcmd.flags = CHAR_STDDEV_READFLAG_PASSMEM | CHAR_STDDEV_READFLAG_ECHO | CHAR_STDDEV_READFLAG_BLOCKING;
			pass_mem(read_cmd->smo, finf->deviceid);
			comm = (int*)&readcmd;
			readcmd.command = CHAR_STDDEV_READ;
			readcmd.dev = finf->logic_deviceid;
			readcmd.buffer_smo = smo;
			readcmd.byte_count = read_cmd->count;
			readcmd.msg_id = wpid;
			readcmd.ret_port = OFS_CHARDEV_PORT;
			break;
		default:
			leave_mutex(&finf->dinf->blocked_mutex);
			return build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND);
	}

	// set blocking command on device
	
	finf->dinf->blocked_command.command = *((struct stdfss_cmd *)read_cmd);	
	finf->dinf->blocked_command.task = thread->taskid;
	finf->dinf->blocked = TRUE;
	
	leave_mutex(&finf->dinf->blocked_mutex);
	
	// send message and continue
	send_msg(finf->deviceid, finf->dinf->protocol_port, comm);
		
	return NULL;
}

struct stdfss_res *chardev_write(int wpid, struct working_thread *thread, struct stdfss_write *write_cmd, int delimited, struct stask_file_info *finf)
{
	struct stdchardev_write_cmd writecmd;
	struct stdchardev_writec_cmd writec;	
	struct stdfss_res *res = NULL;
	struct stdchardev_res deviceres;
	int *comm;

	switch(thread->command.command)
	{
		case STDFSS_PUTS:
			// check device is not already blocked 
			writecmd.flags = CHAR_STDDEV_READFLAG_PASSMEM | CHAR_STDDEV_READFLAG_ECHO | CHAR_STDDEV_READFLAG_DELIMITED;
			pass_mem(write_cmd->smo, finf->deviceid);
			writecmd.command = CHAR_STDDEV_WRITE;
			writecmd.dev = finf->logic_deviceid;
			writecmd.delimiter = '\0';
			writecmd.buffer_smo = write_cmd->smo;
			writecmd.byte_count = write_cmd->count;
			writecmd.msg_id = wpid;
			writecmd.ret_port = OFS_CHARDEV_PORT;
			comm = (int*)&writecmd;
			break;
		case STDFSS_PUTC:
			writec.flags = CHAR_STDDEV_READFLAG_ECHO;
			writec.command = CHAR_STDDEV_WRITEC;
			writec.dev = finf->logic_deviceid;
			writec.c = (char)((struct stdfss_putc *)write_cmd)->c;
			writec.msg_id = wpid;
			writec.ret_port = OFS_CHARDEV_PORT;
			comm = (int*)&writec;
			break;
		case STDFSS_WRITE:
			writecmd.flags = CHAR_STDDEV_READFLAG_PASSMEM | CHAR_STDDEV_READFLAG_ECHO;
			pass_mem(write_cmd->smo, finf->deviceid);
			writecmd.command = CHAR_STDDEV_WRITE;
			writecmd.dev = finf->logic_deviceid;
			writecmd.buffer_smo = write_cmd->smo;
			writecmd.byte_count = write_cmd->count;
			writecmd.msg_id = wpid;
			writecmd.ret_port = OFS_CHARDEV_PORT;
			comm = (int*)&writecmd;
			break;
		default:
			return build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND);
	}

	// send message blocking
	locking_send_msg(finf->deviceid, finf->dinf->protocol_port, comm, wpid);

	get_signal_msg((int*)&deviceres, wpid);

	if(deviceres.ret != STDCHARDEV_OK)
	{
		return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
	}

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}

int check_blocked_device(struct stdchardev_res *charres, int deviceid)
{
	struct stdfss_res *res = NULL;
	char nullbuff = '\0';

	// check if the device is blocked
	struct sdevice_info *dinf = get_cache_device_info(deviceid, charres->dev);

	if(dinf == NULL) return FALSE;

	wait_mutex(&dinf->blocked_mutex);
	if(!dinf->blocked)
	{
		leave_mutex(&dinf->blocked_mutex);
		return FALSE;
	}
	leave_mutex(&dinf->blocked_mutex);

	if(charres->ret != STDCHARDEV_OK)
	{
		res = build_response_msg(dinf->blocked_command.command.command, STDFSSERR_DEVICE_ERROR);
		res->param1 = 0;
		res->param2 = 0;
		res->thr_id = dinf->blocked_command.command.thr_id;
		
		send_msg(dinf->blocked_command.task, dinf->blocked_command.command.ret_port, res);

		free(res);

		dinf->blocked = FALSE;

		return TRUE;
	}
	else
	{
		res = build_response_msg(dinf->blocked_command.command.command, STDFSSERR_OK);
	}

	if(dinf->blocked_command.command.command == STDFSS_GETC)
	{
		((struct stdfss_getc_res*)res)->c = ((struct stdchardev_readc_res*)charres)->c;
	}
	else if(dinf->blocked_command.command.command == STDFSS_GETS)
	{
		write_mem( ((struct stdfss_gets*)&dinf->blocked_command.command)->smo, charres->byte_count-1, 1, &nullbuff); 
	}

	res->param1 = charres->byte_count;
	res->param2 = 0;
	res->thr_id = dinf->blocked_command.command.thr_id;

	wait_mutex(&dinf->blocked_mutex);
	dinf->blocked = FALSE;
	leave_mutex(&dinf->blocked_mutex);

	// ok! it was blocked, send response message to the task
	send_msg(dinf->blocked_command.task, dinf->blocked_command.command.ret_port, res);

	free(res);

	return TRUE;
}


struct stdfss_res *chardev_seek(int wpid, struct working_thread *thread, struct stdfss_seek *seek_cmd, struct stask_file_info *finf)
{
	struct stdfss_res *ret = NULL;
	struct stdchardev_seek_cmd seek;
	struct stdchardev_res deviceres;

	seek.command = CHAR_STDDEV_SEEK;
	switch(seek_cmd->origin)
	{
	case STDFSS_SEEK_SET:
		seek.flags = CHAR_STDDEV_SEEK_SET;
		break;
	case STDFSS_SEEK_CUR:
		seek.flags = CHAR_STDDEV_SEEK_CUR;
		break;
	case STDFSS_SEEK_END:
		seek.flags = CHAR_STDDEV_SEEK_END;
		break;
	default:
		ret = build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND_PARAMS);
		ret->param2 = 1; // set EOF
		return ret;
	}
	
	seek.pos = (int)seek_cmd->possition;
	seek.ret_port = OFS_CHARDEV_PORT;
	seek.msg_id = wpid;
	seek.dev = finf->logic_deviceid;

	locking_send_msg(finf->deviceid, finf->dinf->protocol_port, &seek, wpid);

	get_signal_msg((int*)&deviceres, wpid);

	if(deviceres.ret != STDCHARDEV_OK)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
		ret->param2 = 1; // set EOF
		return ret;
	}

	ret = build_response_msg(thread->command.command, STDFSSERR_OK);
	ret->param2 = 1; // set EOF
	return ret;
}

struct stdfss_res *chardev_tell(int wpid, struct working_thread *thread, struct stdfss_tell *tell_cmd, struct stask_file_info *finf)
{
	struct stdfss_res *ret = NULL;
	struct stdchardev_cmd tell;
	struct stdchardev_res deviceres;

	tell.command = CHAR_STDDEV_SEEK;
	tell.ret_port = OFS_CHARDEV_PORT;
	tell.msg_id = wpid;
	tell.dev = finf->logic_deviceid;

	locking_send_msg(finf->deviceid, finf->dinf->protocol_port, &tell, wpid);

	get_signal_msg((int*)&deviceres, wpid);

	if(deviceres.ret != STDCHARDEV_OK)
	{
		return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
	}

	ret = build_response_msg(thread->command.command, STDFSSERR_OK);

	((struct stdfss_tell_res *)ret)->cursor = deviceres.value1;

	return ret;
}
