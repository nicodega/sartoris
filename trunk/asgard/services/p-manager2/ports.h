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


#ifndef PMANPORTSH
#define PMANPORTSH

#define PMAN_PORT_PROTOCOLS     5    // Ports under this define will be used by Protocols

// PMAN_COMMAND_PORT is 2
// PMAN_SIGNALS_PORT    3       // defined on include/services/pmanager/signals.h
// PMAN_EVENTS_PORT     4       // defined on include/services/pmanager/signals.h

#define TASK_IO_PORT            5
#define THREAD_IO_PORT          6
#define FMAP_IO_PORT            7
#define SHUTDOWN_PORT           8
#define VMM_READ_PORT           9
#define IOSLOT_WRITE_PORT       10
#define IOSLOT_FSWRITE_PORT     11
#define SWAP_TASK_READ_PORT	    12

#define INITFS_PORT   TASK_IO_PORT	// port used for ofs initialization (yeap, it overlaps with TASK_IO_PORT)

#endif
