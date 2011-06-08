/*
*	This header provides functions for opening, reading, writing and seeking on pipes.
*	Pipes provided by this service will be used by the shell. And can be accessed thorugh iolib as files.
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

#ifndef PIPESH
#define PIPESH

#define PIPES_UIID 0x00000007
#define PIPES_VER  1

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#define PIPES_OPENSHARED	1
#define PIPES_OPENFILE		2
#define PIPES_CLOSE		    3
#define PIPES_SEEK		    4
#define PIPES_TELL		    5
#define PIPES_READ		    6
#define PIPES_WRITE		    7
#define PIPES_GETC		    8
#define PIPES_PUTC		    9
#define PIPES_GETS		    10
#define PIPES_PUTS		    11

/* Ret codes */
#define PIPESERR_OK		    0	// ok
#define PIPESERR_PIPECLOSED	1	// pipe has been closed
#define PIPESERR_FSERROR	2	// an error with the file system
#define PIPESERR_EOF		3	// generic error
#define PIPESERR_INVALIDOP	4	// task atempted reading or writing and the operation is not allowed
#define PIPESERR_ERR		-1	// generic error

struct pipes_cmd
{
    unsigned char command;	
    unsigned short thr_id;
    unsigned char specific0;
    unsigned int specific1;
    unsigned int specific2;
    unsigned short specific3;
    unsigned short ret_port;
} PACKED_ATT;

struct pipes_res
{
    unsigned char command;	    // PIPES_OPENFILE or PIPES_OPENSHARED
    unsigned short  thr_id;      // support for multithreading for iolib
    unsigned char  padding0;
    unsigned int param1;
    unsigned int param2;
    unsigned short padding1;
    unsigned short ret;
} PACKED_ATT;

/* Open a pipe between two tasks */
struct pipes_openshared
{
    unsigned char command;	    // PIPES_OPENSHARED
    unsigned short thr_id;
    unsigned char padding0;
    unsigned short task1;
    unsigned short task2;
    unsigned int padding1;
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

/* Open a pipe to a file */
struct pipes_openfile
{
    unsigned char command;     // PIPES_OPENFILE
    unsigned short thr_id;
    unsigned char padding0;
    unsigned short task;        // task for whom the pipe is ment
    unsigned short path_smo;    // smo with the path to the output file
    unsigned char open_mode[6]; // open mode 
    unsigned short ret_port;
} PACKED_ATT;

struct pipes_open_res
{
    unsigned char command;     // PIPES_OPENFILE or PIPES_OPENSHARED
    unsigned short  thr_id;      // support for multithreading for iolib
    unsigned char padding0;
    unsigned int   padding1;
    unsigned int   pipeid;
    unsigned short padding2;	
    unsigned short ret;    
} PACKED_ATT;

/* CLOSE */
struct pipes_close
{
    unsigned char command;	    // PIPES_CLOSE
    unsigned short  thr_id;      // support for multithreading for iolib
    unsigned char  padding0;
    unsigned short pipeid;      // id of the pipe
    unsigned short padding1;
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
} PACKED_ATT;

#define PIPES_SEEK_SET		1		// Beginning of pipe. 
#define PIPES_SEEK_CUR		2		// Current position of the pipe pointer.
#define PIPES_SEEK_END		3		// End of pipe.

struct pipes_seek
{
    unsigned char command;		    /* Command to execute */
    unsigned short thr_id;		    // support for multithreading for iolib
    unsigned char origin;
    unsigned short pipeid;		    // file ID to seek into
    unsigned long long possition;   // possition to seek on file
    unsigned short ret_port;
} PACKED_ATT;

struct pipes_tell
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	
    unsigned short padding1;
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
} PACKED_ATT;

struct pipes_tell_res{
    unsigned char     command; 	/* The command executed */
    unsigned short      thr_id;	    // support for multithreading for iolib
    unsigned char      padding0;
    unsigned long long cursor;	
    unsigned short     padding1;	
    unsigned short     ret;     
} PACKED_ATT;

// PIPES_READ		

struct pipes_read
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	
    unsigned short count;	    // bytes being read
    unsigned int   smo;	        // smo to write read data		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// PIPES_GETS		

struct pipes_gets
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	
    unsigned short count;	    // bytes being read
    unsigned int   smo;	        // smo to write read data		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// PIPES_GETC	

struct pipes_getc
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	    // file ID to read
    unsigned short padding1;	// bytes being read
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
} PACKED_ATT;

struct pipes_getc_res{
    unsigned char command;      /* The command executed */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char c;	        // character read
    unsigned int param1;	
    unsigned int param2;
    unsigned short padding1;	
    unsigned short ret;     
} PACKED_ATT;

// PIPES_WRITE

struct pipes_write
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	
    unsigned short count;       // bytes being written
    unsigned int   smo;	        // smo containing data being written		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// PIPES_PUTS

struct pipes_puts
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	
    unsigned short count;	    // bytes being written
    unsigned int   smo;	        // smo containing data being written		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// PIPES_PUTC

struct pipes_putc
{
    unsigned char command;     /* Command to execute */
    unsigned short thr_id;	    // support for multithreading for iolib
    unsigned char padding0;
    unsigned short pipeid;	 
    unsigned short padding1;	
    unsigned int   c;	        // smo containing data being written		
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

#endif

