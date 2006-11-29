
/*
*   	STANDARD CHAR DEVICE SERVICE MSG FORMAT
*
*	This file defines the standard messages used to comunicate
*	with the Char Device Driver Services and indicates the commands 
*   	that should be provided (if the device does not support a
*   	feature it should return a DEV_NOTSUPPORTED message
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


#include "stddev.h"

#ifndef STDCHARDEVH
#define STDCHARDEVH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

// BLOCKDEV INTERFACE UIID //

#define STD_CHAR_DEV_UIID 0x00000003
#define STD_CHAR_DEV_VER 0x01

// Commands Interface Ver 0x01 //

#define CHAR_STDDEV_READ	1
#define CHAR_STDDEV_WRITE	2	
#define CHAR_STDDEV_READC	3
#define CHAR_STDDEV_WRITEC	4
#define CHAR_STDDEV_RESET	5	
#define CHAR_STDDEV_SEEK	6	// allows seeking while the device is scanning (task must be on blocking read)
#define CHAR_STDDEV_TELL	7

#define STDCHARDEV_OK 1
#define STDCHARDEV_ERR -1

// Generic Message Format //

struct stdchardev_cmd{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char delimiter;	
	unsigned int buffer_smo;
	unsigned int byte_count;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

// Specific message formats //
struct stdchardev_read_cmd
{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char delimiter;
	unsigned int buffer_smo;
	unsigned int byte_count;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

struct stdchardev_readc_cmd
{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char padding0;
	unsigned int padding1;
	unsigned int padding2;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

#define CHAR_STDDEV_READFLAG_ECHO       0x1 // input will be echoed on the screen
#define CHAR_STDDEV_READFLAG_BLOCKING   0x2 // read command wont return unless count bytes are read or the delimiter is found
#define CHAR_STDDEV_READFLAG_DELIMITED  0x4 // consider the delimiter
#define CHAR_STDDEV_READFLAG_PASSMEM    0x8 // if this flag is set, smo will be passed back to the sender when command finishes.

struct stdchardev_write_cmd
{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char delimiter;
	unsigned int buffer_smo;
	unsigned int byte_count;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

struct stdchardev_writec_cmd
{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char c;
	unsigned int padding0;
	unsigned int padding1;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

/* seek will be available when a blocking read is being performed */
#define CHAR_STDDEV_SEEK_SET		1		// Beginning of stream. 
#define CHAR_STDDEV_SEEK_CUR		2		// Current position of the stream pointer.
#define CHAR_STDDEV_SEEK_END		3		// End of stream.

struct stdchardev_seek_cmd
{
	unsigned short command;
	unsigned char dev;		// the logic device ID
	unsigned char padding0;	
	unsigned int padding1;
	unsigned int pos;
	unsigned short flags;
	unsigned char msg_id;
	unsigned char ret_port;
} PACKED_ATT;

// Common Response Message //
struct stdchardev_res{
	unsigned int command;
	unsigned int ret;
	unsigned int byte_count;
	unsigned char padding;
	unsigned char dev;
	unsigned char msg_id;
	unsigned char value1;
} PACKED_ATT;


struct stdchardev_readc_res{
	unsigned int command;
	unsigned int ret;
	unsigned int byte_count;
	unsigned char c;	// character read
	unsigned char dev;
	unsigned char msg_id;
	unsigned char value1;	
} PACKED_ATT;

#endif

