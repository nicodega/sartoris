/*
*	This is the header for the process lib. Every process will have to
*	be linked against this lib, and the entry point should be init. 
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

#ifndef STDPROCESSH
#define STDPROCESSH

#define PACKED_ATT __attribute__ ((__packed__));

#define STDPROCESS_PORT 	0
#define STDPROCESSIOPORT 	1
#define STDPROCESS_INITPORT 	2
#define STDPROCESS_DIRLIBPORT 	3

#define STDPROCESS_STARTPORT	4 	// indicates the first port a programmer can use without 
					// overlapping with the stdlib

#define STDPROCESSERR_OK	0
#define STDPROCESSERR_ERR	1

/* command defines */
#define STDPROCESS_INIT		1
#define STDPROCESS_DIE		-1

/* this message will be sent by the shell 
   when running the process. */
struct stdprocess_init
{
	int command;
	short shell_task;
	short cl_smo;	// smo to the command line 
	short map_smo;	// smo to a struct map_params
	short consoleid;
	int ret_port;
} PACKED_ATT;

/* response */
struct stdprocess_res
{
	int command;
	int padding;
	int padding1;
	int ret;
} PACKED_ATT;

struct map_params
{
	int mapstdin;
	FILE stdin;
	int mapstdout;
	FILE stdout;
	int mapstderr;
	FILE stderr;	
};

#endif
