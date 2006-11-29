/*
* 	STANDARD SIGNAL EMITER MESSAGE FORMAT
*
*	Signal (or event) generators will have to comply to this standard.
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

#ifndef STDSIGNALSH
#define STDSIGNALSH

#define STDSIGNAL_PORT		4
#define STDSIGNAL_VER		0x01
#define STDSIGNAL_UIID		0x00000004

/* MESSAGES */
#define STDSIGNAL_SUSCRIBE 	0 
#define STDSIGNAL_UNSUSCRIBE 	1
#define STDSIGNAL_VERSION	1

/* OUTGOING MESSAGES */
#define STDSIGNAL_SIMPLE_SIGNAL	0
#define STDSIGNAL_SIGNAL	1

/* SUSCRIBE MESSAGE */ 
struct stdsignal_suscribe
{
	int command;	
	int listener_port; 	// por on the sending task where signals will be received
	int signal;		// an int value representing a signal on the generator service
	short params_smo;		// an smo pointing to a generator defined struct for this signal
				// params
	short ret_port;		// port for this message response
};

/* UNSUSCRIBE MESSAGE */ 
struct stdsignal_unsuscribe
{
	int command;
	int signal;
	int padding;
	short padding1;
	short ret_port;
};

/* SIMPLE  SIGNAL */ 
struct stdsignal_simple_signal
{
	int command;
	int signal;
	int param1;
	int param2;
};

/* SIGNAL */ 
struct stdsignal_signal
{
	int command;
	int signal;
	int padding;
	int params_smo;		// an smo to a sctruct defined by the sender, depending on the signal type
};

#endif
