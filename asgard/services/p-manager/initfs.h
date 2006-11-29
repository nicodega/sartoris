/*
*	Process and Memory Manager Service
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


#ifndef INITFSH
#define INITFSH

#define MAX_NAME_SIZE 16

// Init image will have this structure:
//
// Init | InitHeader | Header1 ... HeaderN | Image1 ... ImageN
struct init_header
{
	unsigned int services_count;
};

struct service_header
{
	unsigned int image_size; // size of the image (without counting header size)
	unsigned int image_pos;  // image location from the end of headers
	char image_name[MAX_NAME_SIZE]; 	 // the name of the image (eg. stdconsole, etc)
};

#endif
