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

void term_color_printf(int term, char* format, int color, ...){
    vsprintf(out_buffer, format, (int*) (&color + 1));
    term_color_prints(term, out_buffer, color, BUFFER_SIZE, 0);
}

/* 
 * Prints on the console waiting for console confirmation.
 */
void term_print(int term, char* str){
    term_color_print(term, str, 0x07);
}

void term_color_print(int term, char* str, int color){
    term_color_prints(term, str, color, BUFFER_SIZE, 1);
}

void term_color_prints(int term, char* str, int color, int size, int buff)
{
	int smo;
	int id;
	struct stdchardev_res csl_res;
  	struct stddev_ioctrl_res ioctlres;
	struct stdchardev_write_cmd csl_io;
  	struct stddev_ioctl_cmd ioctl;

	if(size == 0 || str == NULL) return;

	if(buff)
	{
		strcp(out_buffer, str);	
		smo = share_mem(CONS_TASK, out_buffer, BUFFER_SIZE, READ_PERM);
	}
	else
	{
		smo = share_mem(CONS_TASK, str, size, READ_PERM);
	}

	// change att using IOCTRL
	ioctl.command = STDDEV_IOCTL;
	ioctl.request = CSL_IO_SETATT;
	ioctl.param = color;
	ioctl.logic_deviceid = term;
	ioctl.msg_id = 100+term;
	ioctl.ret_port = CSL_ACK_PORT;

	// write  
	csl_io.command = CHAR_STDDEV_WRITE;
	csl_io.buffer_smo = smo;
	csl_io.byte_count = size;
	csl_io.msg_id = 100+term;
	csl_io.dev = term;
	csl_io.ret_port = CSL_ACK_PORT;
	csl_io.delimiter = '\0';
	csl_io.flags = CHAR_STDDEV_READFLAG_DELIMITED;

	send_msg(CONS_TASK, console_stddev_port, &ioctl);

	while (get_msg_count(CSL_ACK_PORT)==0){ reschedule(); }

	get_msg(CSL_ACK_PORT, &ioctlres, &id);

	send_msg(CONS_TASK, console_stdchardev_port, &csl_io);

	while (get_msg_count(CSL_ACK_PORT)==0){ reschedule(); }

	get_msg(CSL_ACK_PORT, &csl_res, &id);

	claim_mem(smo);
}

/* 
 * Clear Screen
 */
void term_clean(int term)
{
	struct stdchardev_cmd csl_io;
	struct stdchardev_res csl_res;
	int id;

	csl_io.command = CHAR_STDDEV_RESET;
	csl_io.msg_id = 100+term;
	csl_io.dev = term;
	csl_io.ret_port = CSL_ACK_PORT;

	send_msg(CONS_TASK, console_stdchardev_port, &csl_io);

	while (get_msg_count(CSL_ACK_PORT)==0){ reschedule(); }
	get_msg(CSL_ACK_PORT, &csl_res, &id);   
}
int k = 0;
/*
 * Show console prompt and wait for command. (Non blocking)
 */
void show_prompt(int term)
{
	strcp(str_buffer, "");
	strcat(str_buffer, user_name);
	strcat(str_buffer, "@");
	strcat(str_buffer, machine_name);
	strcat(str_buffer, ":");
	strcat(str_buffer, csl_dir[term]);
	strcat(str_buffer, "# ");
	term_print(term, str_buffer);
	term_prepare_input(term, true);
}

/*
 * Tell console to wait for user input. (Non Blocking)
 */
void term_prepare_input(int term, char echo)
{
	struct stdchardev_read_cmd csl_io;

	pass_mem(csl_smo[term], CONS_TASK);

	csl_io.command = CHAR_STDDEV_READ;
	csl_io.delimiter = '\n';
	csl_io.flags = CHAR_STDDEV_READFLAG_ECHO | CHAR_STDDEV_READFLAG_BLOCKING | CHAR_STDDEV_READFLAG_DELIMITED;
	csl_io.msg_id = term;
	csl_io.byte_count = MAX_COMMAND_LINE_LEN;
	csl_io.buffer_smo = csl_smo[term];
	csl_io.dev = term;
	csl_io.ret_port = CSL_SCAN_ACK_PORT;

	if(send_msg(CONS_TASK, console_stdchardev_port, &csl_io) < 0)
    {
        string_print("SHELL COULD NOT SEND CONSOLE MESSAGE",160*2 + 80,12);
    }
}

int get_console(int term)
{
	struct stddev_getdev_cmd getdev_cmd;
	struct stddev_res stddevres;
	int id;

	// get console ownership
	getdev_cmd.command = STDDEV_GET_DEVICE;
	getdev_cmd.ret_port = CSL_REQUEST_PORT;
	getdev_cmd.logic_deviceid = term;
	getdev_cmd.msg_id = term;

	send_msg(CONS_TASK, console_stddev_port, &getdev_cmd);

	while (get_msg_count(CSL_REQUEST_PORT)==0){ reschedule(); }

	get_msg(CSL_REQUEST_PORT, &stddevres, &id);

	if(stddevres.ret == STDDEV_ERR)
	{
		return 0;
	}

	return 1;
}
int free_console(int term)
{
	struct stddev_res stddevres;
	struct stddev_freedev_cmd freedev_cmd;
	int id;

	// frees the specified console
	freedev_cmd.command = STDDEV_FREE_DEVICE;
	freedev_cmd.ret_port = CSL_REQUEST_PORT;
	freedev_cmd.logic_deviceid = term;
	freedev_cmd.msg_id = term;

	send_msg(CONS_TASK, console_stddev_port, &freedev_cmd);

	while (get_msg_count(CSL_REQUEST_PORT)==0){ reschedule(); }

	get_msg(CSL_REQUEST_PORT, &stddevres, &id);

	if(stddevres.ret == STDDEV_ERR)
	{
		return 0;
	}

	return 1;
}
