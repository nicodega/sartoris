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

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <services/directory/directory.h>
#include <lib/structures/string.h>
#include <lib/structures/lptree.h>
#include <lib/structures/avltree.h>
#include <lib/malloc.h>
#include <lib/const.h>
#include <lib/scheduler.h>
#include <services/stds/stdservice.h>

#ifndef DIRECTORYINTERNALSH
#define DIRECTORYINTERNALSH

struct directory_cmd
{
	int command;
	int specific0;
	int thr_id;
	int ret_port;
};

struct servdirectory_entry
{
	char *service_name;
	int serviceid;
};

#ifdef WIN32DEBUGGER
DWORD WINAPI directory_main(LPVOID lpParameter);
#else
void directory_main();
#endif

void process_query_interface(struct stdservice_query_interface *query_cmd, int task);

#endif
