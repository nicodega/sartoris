
/*
*   	STANDARD BLOCK DEVICE SERVICE MSG FORMAT
*
*	This file defines the standard messages used to comunicate
*	with the Block Device Driver Services and indicates the commands 
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

#ifndef STDBLOCKDEVH
#define STDBLOCKDEVH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

// BLOCKDEV INTERFACE UIID //

#define STD_BLOCK_DEV_UIID 0x00000002
#define STD_BLOCK_DEV_VER 0x01

// Commands Interface Ver 0x01 //

// this commands are issued on logic devices
#define BLOCK_STDDEV_READ	1
#define BLOCK_STDDEV_WRITE	2	
#define BLOCK_STDDEV_READM	4
#define BLOCK_STDDEV_WRITEM	16
#define BLOCK_STDDEV_INFO	32 // this command does not require device ownership

// if a logic device ID masked with the following 
// flag is non zero, reference will be to a physical device
// numbered by ID & !PHDEV_FLAG
#define STDDEV_PHDEV_FLAG 0x8000

#define BLOCK_STDDEV_RWREAD	BLOCK_STDDEV_READ | RAWOP_FLAG
#define BLOCK_STDDEV_RWWRITE	BLOCK_STDDEV_WRITE | RAWOP_FLAG
#define BLOCK_STDDEV_RWREADM	BLOCK_STDDEV_READM | RAWOP_FLAG
#define BLOCK_STDDEV_RWWRITEM	BLOCK_STDDEV_WRITEM | RAWOP_FLAG 
#define BLOCK_STDDEV_RWINFO	BLOCK_STDDEV_INFO | RAWOP_FLAG

// ret values //

#define STDBLOCKDEV_OK 1
#define STDBLOCKDEV_ERR -1

// extended errors //
#define STDBLOCKDEVERR_NOERROR			0x00
#define STDBLOCKDEVERR_INCORRECTOPCODE		0x01
#define STDBLOCKDEVERR_INVALIDDISKNUMBER	0x02
#define STDBLOCKDEVERR_INVALIDSTARTSECTOR	0x03
#define STDBLOCKDEVERR_TOOLONGCOUNTER		0x04
#define STDBLOCKDEVERR_SMOERROR			0x05
#define STDBLOCKDEVERR_OPERATIONABORTED		0x06
#define STDBLOCKDEVERR_BADBLOCKMARK		0x07
#define STDBLOCKDEVERR_UNCORRECTABLEDATAERROR	0x08
#define STDBLOCKDEVERR_TRACKZERONOTFOUND	0x09
#define STDBLOCKDEVERR_IMPOSIBLETOWRITESECTOR	0x0A
#define STDBLOCKDEVERR_IMPOSIBLETOREADSECTOR	0x0B
#define STDBLOCKDEVERR_UNKNOWNERROR		0x0C
#define STDBLOCKDEVERR_DISKNOTPRESENT		0x0D
#define STDBLOCKDEVERR_NOSPACEINQUEUE		0x0E
#define STDBLOCKDEVERR_ERRORINITIALIZINGDRIVER	0x0F
#define STDBLOCKDEVERR_INCORRECTPORTTOREQUEST	0x10
#define STDBLOCKDEVERR_REQUESTEDSECTORNOTFOUND	0x11
#define	STDBLOCKDEVERR_DATAADDMARKNOTFOUND	0x12
#define STDBLOCKDEVERR_DRIVERSTILLBUSY		0x13
#define STDBLOCKDEVERR_TIMEOUTERROR		0x14
#define STDBLOCKDEVERR_WRONG_LOGIC_DEVICE 	0x15
#define STDBLOCKDEVERR_INVALIDIOCTRL		0x16

// Generic Message Format //

struct stdblockdev_cmd{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int buffer_smo;
	unsigned int pos;
	unsigned short padding;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;

// Specific message formats //

struct stdblockdev_read_cmd
{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int buffer_smo;
	unsigned int pos;
	unsigned short padding;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;

struct stdblockdev_write_cmd
{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int buffer_smo;
	unsigned int pos;
	unsigned short padding;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;

struct stdblockdev_readm_cmd
{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int buffer_smo;
	unsigned int pos;
	unsigned short count;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;

struct stdblockdev_writem_cmd
{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int buffer_smo;
	unsigned int pos;
	unsigned short count;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;


// Common Response Message //

struct stdblockdev_res
{
	unsigned short command;
	unsigned int ret;
	unsigned int extended_error;
	unsigned short dev;
	unsigned short msg_id;
	unsigned char value1;
	unsigned char value2;
} PACKED_ATT;

struct stdblockdev_devinfo_cmd{
	unsigned char command;
	unsigned short dev;			// the logic device ID
	unsigned int devinf_smo;
	unsigned int devinf_id;		// id of the structure on the smo
	unsigned short padding;
	unsigned short msg_id;		// used for non ordered protocol
	unsigned char ret_port;
} PACKED_ATT;

// DEVICE INFO STRUCTURE
#define DEVICEINFO_ID 0x00000001

struct blockdev_info0
{
	unsigned long long media_size;
	unsigned long max_lba;
	unsigned int metadata_lba_end;
};

#endif

