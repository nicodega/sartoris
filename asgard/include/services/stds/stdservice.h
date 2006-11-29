/**************************************************************
*   	STANDARD SERVICE MSG FORMAT
*
*	This file defines the standard messages sent to services.
*	All system services must implement this interface.
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

#ifndef STDSERVICEH
#define STDSERVICEH

#define STDSERVICE_PORT				0


#define STDSERVICE_DIE				0 // clean die
#define STDSERVICE_FORCEDIE			1 // die no matter how
#define STDSERVICE_PREPAREFORSLEEP	2 // sent before the system is hibernated
#define STDSERVICE_WAKEUP			3 // sent before starting the service when recovering from hibernation
#define STDSERVICE_QUERYINTERFACE	4 // with this command it's possible to obtain information on interfaces 
									  // provided through messages by services. 

#define STDSERVICE_RESPONSE_OK		0
#define STDSERVICE_RESPONSE_FAILED	1

struct stdservice_cmd{
	int command;
	int padding0;
	int padding1;
	short padding2;
	short ret_port;
} PACKED_ATT;

struct stdservice_query_interface{
	int command;
	int uiid;			// Unique Interface IDentifier
	unsigned int ver;		// interface version
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short ret_port;	
} PACKED_ATT;

struct stdservice_res
{
	unsigned int command;		/* The command executed */
	unsigned int ret;
	unsigned int padding0;		
	unsigned short msg_id;		
	unsigned short padding1;
} PACKED_ATT;

// Query interface Response Message //
struct stdservice_query_res
{
	unsigned int command;		/* The command executed */
	unsigned int ret;
	unsigned int port;		// port where the interface is supported
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short padding0;
} PACKED_ATT;

#endif

