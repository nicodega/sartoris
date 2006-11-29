/*
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


#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#ifndef SHELL_H
#define SHELL_H

#define SHELL_PORT	1

#define SHELL_GETENV	1	// gets an environment variable value
#define SHELL_SETENV	2	// sets an environment variable

#define SHELLERR_OK			0
#define SHELLERR_SMO_TOOSMALL		1
#define SHELLERR_INVALIDNAME		2
#define SHELLERR_INVALIDVALUE		3
#define SHELLERR_INVALIDCOMMAND		4
#define SHELLERR_SMOERROR		5
#define SHELLERR_VAR_NOTDEFINED		6

/* generic command format */
struct shell_cmd
{
	int command;
	short global;
	short name_smo;
	short value_smo;
	short msg_id;
	int ret_port;
} PACKED_ATT;

/* response format */
struct shell_res
{
	int command;
	int padding;
	short padding1;
	short msg_id;
	int ret;
} PACKED_ATT;

/* specific commands */

struct shell_getenv
{
	int command;
	short padding;
	short name_smo;
	short value_smo;
	short msg_id;
	int ret_port;
} PACKED_ATT;

struct shell_setenv
{
	int command;
	short global;
	short name_smo;
	short value_smo;
	short msg_id;
	int ret_port;
} PACKED_ATT;

#endif
