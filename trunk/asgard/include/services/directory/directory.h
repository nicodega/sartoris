/*
*
*	DIRECTORY Service
*
*	This is a temporary implementation of a directory service.
*	It will asociate tasks with a name string.
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

#include <lib/structures/string.h>

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#ifndef DIRECTORYH
#define DIRECTORYH

#define DIRECTORY_PORT 1

#define DIRECTORY_UIID 	0x00000005
#define DIRECTORY_VER	1


#define DIRECTORY_REGISTER_SERVICE	1
#define DIRECTORY_UNREGISTER_SERVICE	2
#define DIRECTORY_RESOLVEID		3
#define DIRECTORY_RESOLVENAME		4

#define DIRECTORYERR_OK			0
#define DIRECTORYERR_ALREADYREGISTERED	1
#define DIRECTORYERR_NOTREGISTERED	2
#define DIRECTORYERR_SMO_TOOSMALL	3

struct directory_register
{
	int command;
	int service_name_smo;
	int thr_id;
	int ret_port;
}PACKED_ATT;

struct directory_unregister
{
	int command;
	int padding0;
	int thr_id;
	int ret_port;
}PACKED_ATT;

struct directory_resolveid
{
	int command;
	int service_name_smo;
	int thr_id;
	int ret_port;
}PACKED_ATT;

struct directory_resolvename
{
	int command;
	short serviceid;
	short name_smo;
	int thr_id;
	int ret_port;
}PACKED_ATT;


struct directory_response
{
	int command;
	int ret_value;	
	int thr_id;
	int ret;
}PACKED_ATT;

#endif

