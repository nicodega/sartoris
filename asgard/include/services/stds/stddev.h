/*
*   	STANDARD DEVICE SERVICE MSG FORMAT
*
*	This file defines the standard messages used to comunicate
*	with Device Driver Services and indicates the commands 
*   	that should be provided (if the device does not support a
*   	feature it should return a DEV_NOTSUPPORTED message)
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

#ifndef STDDEVH
#define STDDEVH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

// STDDEV Protocol Version Number //

//#define STDDEV_PORT		1
#define STDDEV_VERSION		0x01
#define STDDEV_UIID		0x00000001

// Base Device Interface (Every Device service MUST implement it //

#define STDDEV_GET_DEVICETYPE	1
#define STDDEV_VER		2	// get's the stddev interface version (this always has to be implemented)
#define STDDEV_GET_DEVICE	3	// Gets a device, if it ´s dedicated, it-ll return a busy error on request if its already requested
#define STDDEV_GET_DEVICEX	4	// gets a device in eXclusive mode
#define STDDEV_FREE_DEVICE	5	// FREES a device (mostly usefull with dedicated devices...)
#define STDDEV_TEST_COMMAND	8	// Used to query the device driver in order to know if it supports a given command on an interface
#define STDDEV_IOCTL		9 	// used for sending IOCTL messages to devices drivers

// Response Codes //

#define STDDEV_OK				1
#define STDDEV_NOTSUPPORTED		-2
#define STDDEV_ERR				-1


// Base Interface :: Message format //

struct stddev_cmd{
	unsigned int command;
		/* Command to execute */
	unsigned short padding0;
	unsigned short logic_deviceid;
	unsigned int padding1;
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

struct stddev_getdev_cmd{
	unsigned int command;
		/* Command to execute */
	unsigned short padding0;
	unsigned short logic_deviceid;
	unsigned int padding1;
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

struct stddev_freedev_cmd{
	unsigned int command;
		/* Command to execute */
	unsigned short padding0;
	unsigned short logic_deviceid;
	unsigned int padding1;
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

// Response Message //

struct stddev_res{
	unsigned int command;
		/* The command executed */
	unsigned int ret;
	unsigned int padding0;
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short logic_deviceid;
} PACKED_ATT;

/* IOCTL */
struct stddev_ioctl_cmd{
	unsigned int command;
		/* Command to execute */
	unsigned short request;			// the requested command for the device
	unsigned short logic_deviceid;	// the logic device on which IO will be issued
	unsigned int param;				// param could be a number or an smo, it will depend on the command
	unsigned short msg_id;			// this field must be copied on the ret msg as it comes
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

// For the Get device type command the response message will be //

// device types //
// NOTE: More device types could be added here in a future
#define STDDEV_UNDEFINED STDDEV_ERR
#define STDDEV_BLOCK 1
#define STDDEV_CHAR 2

struct stddev_devtype_res{
	unsigned int command;			// STDDEV_GET_DEVICETYPE
		/* The command executed */
	unsigned int ret;		
	unsigned int dev_type;			// DEVICE TYPE
	unsigned short msg_id;			// this field must be copied on the ret msg as it comes
	unsigned short block_size;		// if it's a block dev, it'll return block size here
} PACKED_ATT;

// For the VER command response will have the format: //

struct stddev_ver_res{
	unsigned int command;			// STDDEV_VER
		/* The command executed */
	unsigned int ret;		
	unsigned int ver;				// PROTOCOL VERSION
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short padding0;
} PACKED_ATT;

struct stddev_ioctrl_res{
	unsigned int command;
		/* The command executed */
	unsigned int ret;
	unsigned int dev_error;
	unsigned short msg_id;		// this field must be copied on the ret msg as it comes
	unsigned short logic_deviceid;
} PACKED_ATT;

#endif
