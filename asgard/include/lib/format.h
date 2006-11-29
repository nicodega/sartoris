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

#ifndef FORMATH
#define FORMATH

#define FORMAT_SIMPLE_OFS_TYPE 1

#define FORMAT_PORT 1

extern int format_port;

void set_format_port(int port);
int format(char *device_file_name, int type, void *params, int params_count, void (*print_func)(char*,int));

#endif
