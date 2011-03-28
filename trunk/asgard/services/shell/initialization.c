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

/*
 *  Inint consoles, and internal structures mantained for them
 */

void register_signal(int code, int term);

void init_consoles()
{
	struct stddev_ioctl_cmd ioctl;
	struct stddev_ioctrl_res ioctlres;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
  	int i, id;

	// resolve console ports
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = CSL_REQUEST_PORT;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(CONS_TASK, STDSERVICE_PORT, &query_cmd);

	while (get_msg_count(CSL_REQUEST_PORT)==0){ reschedule(); }
      
	get_msg(CSL_REQUEST_PORT, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		for(;;); // this should never happen
	}

	console_stddev_port = query_res.port;

	query_cmd.uiid = STD_CHAR_DEV_UIID;
	query_cmd.ver = STD_CHAR_DEV_VER;

	send_msg(CONS_TASK, STDSERVICE_PORT, &query_cmd);

	while (get_msg_count(CSL_REQUEST_PORT)==0){ reschedule(); }
    
	get_msg(CSL_REQUEST_PORT, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		for(;;); // this should never happen
	}

	console_stdchardev_port = query_res.port;
    
	// now initialize each console
	for (i = 0; i < NUM_TERMS; i++)
	{
		argc[i] = 0;

		// we should get console ownership here
		get_console(i);

		cslown[i].mode = SHELL_CSLMODE_SHELL; // console is taken by the shell
		cslown[i].process = -1;
		cslown[i].batched_index = -1;

		csl_lastcmdcnt[i] = 0;
		strcp(csl_dir[i], "/");
		csl_smo[i] = share_mem(CONS_TASK, csl_cmd[i], MAX_COMMAND_LEN, WRITE_PERM);
    
		term_clean(i);
		term_print(i, txt_init);
		term_print(i, txt_csl[i]);
		term_print(i, "\n\n");
		show_prompt(i);

		// set signal port
		ioctl.command = STDDEV_IOCTL;
		ioctl.request = CSL_IO_SETSIGNALPORT;
		ioctl.param = CSL_SIGNAL_PORT;
		ioctl.logic_deviceid = i;
		ioctl.msg_id = 100+i;
		ioctl.ret_port = CSL_ACK_PORT;

		send_msg(CONS_TASK, console_stddev_port, &ioctl);

		while (get_msg_count(CSL_ACK_PORT)==0){ reschedule(); }
        
		get_msg(CSL_ACK_PORT, &ioctlres, &id);

		// register key up and down signals
		register_signal(0x90, i);	// UP
		register_signal(0x91, i);	// DOWN
		register_signal('c', i);	// c, for Ctrl + c
  	}
}

void register_signal(int code, int term)
{
	struct stddev_ioctl_cmd ioctl;
	struct stddev_ioctrl_res ioctlres;
	int id;

	ioctl.command = STDDEV_IOCTL;
	ioctl.request = CSL_IO_SIGNAL;
	ioctl.param = code; 
	ioctl.logic_deviceid = term;
	ioctl.msg_id = 100+term;
	ioctl.ret_port = CSL_ACK_PORT;
	
	send_msg(CONS_TASK, console_stddev_port, &ioctl);

	while (get_msg_count(CSL_ACK_PORT)==0){ reschedule(); }

	get_msg(CSL_ACK_PORT, &ioctlres, &id);
}

