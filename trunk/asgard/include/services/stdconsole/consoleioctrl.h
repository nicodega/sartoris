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

#ifndef _CONSOLEIOCTRL_H_
#define _CONSOLEIOCTRL_H_

#define CSL_IO_SWITCH 	1
#define CSL_IO_SETATT 	2
#define CSL_IO_SETSIGNALPORT 	3
#define CSL_IO_SIGNAL 	4
#define CSL_IO_USIGNAL 	5
#define CSL_IO_DISABLE_ECHO 	6 // this ioctl will override stdchar dev msgs echo
#define CSL_IO_ENABLE_ECHO 	7 // this ioctl will override stdchar dev msgs echo
#define CSL_IO_COMMAND_ECHO 	8 

/* This will be the message sent by the console when a singaled key is pressed */
#define CSL_SIGNAL 	1

struct csl_key_signal
{
	int command;
	char key;
	char mask;
	short padding0;
	int padding1;
	int console;
} PACKED_ATT;

#endif
