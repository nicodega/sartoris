/*
*	DIRLIB 0.1: A common library for using the directory service.
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

#include <lib/dirlib.h>

int dirlib_port = DIRLIB_PORT;

void dir_set_port(int port)
{
	dirlib_port = port;
}

int dir_register(char *service_name)
{
	struct directory_register reg_cmd;
	struct directory_response dir_res;
	int id_proc;

	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
 	reg_cmd.ret_port = dirlib_port;
	reg_cmd.thr_id = get_current_thread();
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name) + 1, READ_PERM);
	
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd);
  
	while (get_msg_count(dirlib_port) == 0) { reschedule(); }

	get_msg(dirlib_port, &dir_res, &id_proc);

	claim_mem(reg_cmd.service_name_smo);

	if(dir_res.ret != DIRECTORYERR_OK) return 1; // fail

	return 0;
}

int dir_unregister()
{
	struct directory_unregister reg_cmd;
	struct directory_response dir_res;
	int id_proc;

	reg_cmd.command = DIRECTORY_UNREGISTER_SERVICE;
 	reg_cmd.ret_port = dirlib_port;
	reg_cmd.thr_id = get_current_thread();
	
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd);
  
	while (get_msg_count(dirlib_port) == 0) { reschedule(); }

	get_msg(dirlib_port, &dir_res, &id_proc);

	if(dir_res.ret != DIRECTORYERR_OK) return 1; // fail

	return 0;
}

int dir_resolveid(char *service_name)
{
	struct directory_resolveid res_cmd;
	struct directory_response dir_res;
	int id_proc;

	res_cmd.command = DIRECTORY_RESOLVEID;
 	res_cmd.ret_port = dirlib_port;
	res_cmd.thr_id = get_current_thread();
	res_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name) + 1, READ_PERM);
	
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &res_cmd);
  
	while (get_msg_count(dirlib_port) == 0) { reschedule(); }

	get_msg(dirlib_port, &dir_res, &id_proc);

	claim_mem(res_cmd.service_name_smo);

	if(dir_res.ret != DIRECTORYERR_OK) return -1; // fail

	return dir_res.ret_value;
}

char *resolve_name(int serviceid)
{
	struct directory_resolvename res_cmd;
	struct directory_response dir_res;
	char *service_name = "";
	int id_proc;

	res_cmd.command = DIRECTORY_RESOLVENAME;
	res_cmd.serviceid = serviceid;
 	res_cmd.ret_port = dirlib_port;
	res_cmd.thr_id = get_current_thread();
	// we will send an smo with size 1 first
	// servicename will not fit and directory will return
	// an error spacifying dir name size.
	res_cmd.name_smo = share_mem(DIRECTORY_TASK, service_name, 1, WRITE_PERM); 
	
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &res_cmd);
  
	while (get_msg_count(dirlib_port) == 0) { reschedule(); }

	get_msg(dirlib_port, &dir_res, &id_proc);

	claim_mem(res_cmd.name_smo);	

	if(dir_res.ret == DIRECTORYERR_SMO_TOOSMALL)
	{
		// now malloc for servicename
		service_name = (char *)malloc(dir_res.ret_value + 1);

		res_cmd.name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name) + 1, WRITE_PERM); 

		send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &res_cmd);
  
		while (get_msg_count(dirlib_port) == 0) { reschedule(); }

		get_msg(dirlib_port, &dir_res, &id_proc);

		claim_mem(res_cmd.name_smo);	

		if(dir_res.ret != DIRECTORYERR_OK) 
		{
			free(service_name);
			return NULL; // fail
		}

		return service_name;
	}
	else
	{
		return NULL; // fail
	}
}

