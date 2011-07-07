/**************************************************************
*   	STANDARD FILE SYSTEM SERVICE MSG FORMAT
*
*	This file defines the standard messages used to comunicate
*	with the File System Services and indicates the commands 
*   	that should be provided (if the service does not support a
*   	feature it should return an STDFSSERR_NOTSUPPORTED message
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

#ifndef STDFSSH
#define STDFSSH

#define STDFSS_PORT		1
#define STDFSS_VER		0x01

#define STDFSS_EMPTY_PARAM 0

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

// Commands Interface //

#define STDFSS_VERSION      0
#define STDFSS_MOUNT        1
#define STDFSS_UMOUNT       2
#define STDFSS_OPEN         3
#define STDFSS_CLOSE		4
#define STDFSS_SEEK         5
#define STDFSS_READ         6
#define STDFSS_WRITE        7
#define STDFSS_FLUSH        8
#define STDFSS_LINK         9
#define STDFSS_MKDIR        10
#define STDFSS_MKDEVICE     11
#define STDFSS_CHANGEATT    12
#define STDFSS_DELETE       13
#define STDFSS_FILEINFO		14
#define STDFSS_INIT         15
#define STDFSS_EXISTS       16
#define STDFSS_IOCTL        17
#define STDFSS_TELL         18
#define STDFSS_GETC         19
#define STDFSS_PUTC         20
#define STDFSS_GETS         21
#define STDFSS_PUTS         22
#define STDFSS_FORWARD		23
#define STDFSS_TAKEOVER		24		// Take over will be issued by PMAN to take control of a file
#define STDFSS_RETURN		25		// Return will return control over a file to a taken over task

// Response Codes //

#define STDFSSERR_OK						0
#define STDFSSERR_DEVICE_NOT_MOUNTED		1
#define STDFSSERR_PATH_ALREADY_MOUNTED		2
#define STDFSSERR_DEVICE_ALREADY_MOUNTED	3
#define STDFSSERR_DEVICE_FILE_NOT_FOUND		4
#define STDFSSERR_SERVICE_NOT_INITIALIZED	5
#define STDFSSERR_NOT_ENOUGH_MEM			6
#define STDFSSERR_COULD_NOT_ACCESS_DEVICE	7
#define STDFSSERR_STDFS_SERVICE_SHUTDOWN	8
#define STDFSSERR_SERVICE_ALREADY_INITIALIZED 	9
#define STDFSSERR_INTERFACE_NOT_FOUND		10
#define STDFSSERR_DEVICE_ERROR				11
#define STDFSSERR_FORMAT_NOT_SUPPORTED		12
#define STDFSSERR_INVALID_COMMAND			13
#define STDFSSERR_INVALID_COMMAND_PARAMS	14
#define STDFSSERR_EOF						15
#define STDFSSERR_FATAL						16
#define STDFSSERR_NOT_FOUND					17
#define STDFSSERR_FILE_INUSE				18
#define STDFSSERR_FILE_DOESNOTEXIST			19
#define STDFSSERR_FILE_INVALIDOPERATION		20
#define STDFSSERR_FILE_DIRNOTEMPTY			21
#define STDFSSERR_FILE_NOTOPEN				22
#define STDFSSERR_TOOMANY_FILESOPEN			23
#define STDFSSERR_DEVICE_MOUNTED			24
#define STDFSSERR_PATH_INUSE				25
#define STDFSSERR_BAD_DEVICE_FILE			26
#define STDFSSERR_DEVICE_INUSE				27
#define STDFSSERR_FILE_EXISTS				28
#define STDFSSERR_DEVICE_NOTSUPPORTED		29
#define STDFSSERR_NOT_DEVICEFILE			30
#define STDFSSERR_NOT_SUPPORTED				31
#define STDFSSERR_NO_PRIVILEGES				32
#define STDFSSERR_SMO_ERROR					33
#define STDFSSERR_FS_ERROR					34
#define STDFSSERR_COMMAND_NOTSUPPORTED		35
#define STDFSSERR_FILENAME_TOOLONG			36
#define STDFSSERR_INVALID_PATH				37

/* OPEN MODES */

#define STDFSS_FILEMODE_READ		0x1
#define STDFSS_FILEMODE_WRITE		0x2
#define STDFSS_FILEMODE_APPEND		0x4
#define STDFSS_FILEMODE_EXCLUSIVE	0x8


/* OPEN FLAGS */

#define STDFSS_OPEN_WRITEBUFFERED	0x1		// If specified, operations might get delayed. If not, operations will be persisted when done.
#define STDFSS_FILEMODE_MUSTEXIST	0x10

// Message format //

struct stdfss_cmd{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char specific0;
    unsigned int   specific1;
    unsigned int   specific2;
    unsigned short specific3;
    unsigned short ret_port;	/* The port for the response message */
} PACKED_ATT;

struct stdfss_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int param1;	
    unsigned int param2;
    unsigned short padding1;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

// Takeover response
struct stdfss_takeover_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char perms;        // current file permissions
    unsigned int file_id;	    // new File ID assigned to the process
    unsigned int unique_id;     // a unique ID for the file on the device it belongs to.
    unsigned short dev;         // the device the file belongs to. (together with unique_id they must provide a unique id for the file)
    unsigned short ret;         /* return value (see OFS_RET defines) */
} PACKED_ATT;

// stdfss_ver ret message structure

struct stdfss_ver_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   padding1;
    unsigned int   ver;
    unsigned short padding2;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

// stdfss_open response message

struct stdfss_open_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   padding1;
    unsigned int   file_id;
    unsigned short padding2;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

struct stdfss_tell_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned long long cursor;	
    unsigned short padding1;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

// stdfss_open response message	

struct stdfss_exists_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   type;
    unsigned int   padding1;
    unsigned short padding2;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

struct stdfss_ioctl_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   dev_ret;
    unsigned int   padding1;
    unsigned short padding2;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

struct stdfss_getc_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char c;	// character read
    unsigned int param1;	
    unsigned int param2;
    unsigned short padding1;	// used when forwarding
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

/* Next response will only be issued between FS services */
struct stdfss_forward_res{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int param1;	
    unsigned int param2;
    unsigned short taskid;	// id of the task originating the call
    unsigned short ret;     /* return value (see OFS_RET defines)*/
} PACKED_ATT;

/*
Custom Message Structures
*/

/* FORWARD */
struct stdfss_forward{
    unsigned char  command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char  specific0;
    unsigned int   specific1;
    unsigned int   specific2;
    unsigned short specific3;
    unsigned short ret_port;
    /* The port for the response message */
} PACKED_ATT;


/* IOCTL */
struct stdfss_ioctl{
    unsigned char  command;      /* Command to execute */
    unsigned short thr_id;      // support for multithreading for iolib
    unsigned char  padding0;
    unsigned short file_id;     // id of the file on which ioctl is to be performed
    unsigned int   request;     // the requested command for the device
    unsigned int   param;       // param could be a number or an smo, it will depend on the command
    unsigned short ret_port;    // The port for the response message
}PACKED_ATT;

// EXISTS
struct stdfss_exists{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   path_smo;
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
    /* The port for the response message */
} PACKED_ATT;

// STDFSS_MOUNT

// MOUNT MODES //

#define STDFSS_MOUNTMODE_DEDICATED	0x1
#define STDFSS_MOUNTMODE_READ		0x2
#define STDFSS_MOUNTMODE_WRITE		0x4

struct stdfss_mount{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int dev_path;		// smo of device file path (eg: /dev/fd0) (the device **MUST** be in the same FS)
    unsigned int mount_path;	// smo of mount path (eg: /mnt/floppy)
    unsigned short mode;		// mount mode as defined previously on "MOUNT MODES"
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_UMOUNT

struct stdfss_umount
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   mount_path;	// smo of mount path to umount
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_OPEN

struct stdfss_open
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   file_path;	// smo of file path
    unsigned int   open_mode; // open mode string "r,w,a,x"
    unsigned short flags;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_CLOSE	

struct stdfss_close
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID to close
    unsigned short padding1;
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
}PACKED_ATT;

// STDFSS_TELL

struct stdfss_tell
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID to close
    unsigned short padding1;
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_SEEK		

#define STDFSS_SEEK_SET		1		// Beginning of file. 
#define STDFSS_SEEK_CUR		2		// Current position of the file pointer.
#define STDFSS_SEEK_END		3		// End of file.

struct stdfss_seek
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char origin;
    unsigned short file_id;		// file ID to seek into
    unsigned long long possition; // possition to seek on file
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_READ		

struct stdfss_read
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID to read
    unsigned short count;	// bytes being read
    unsigned int   smo;		// smo to write read data		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_GETS		

struct stdfss_gets
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID to read
    unsigned short count;	// bytes being read
    unsigned int   smo;		// smo to write read data		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_GETC	

struct stdfss_getc
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID to read
    unsigned short padding1;	// bytes being read
    unsigned int   padding2;
    unsigned short padding3;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_WRITE

struct stdfss_write
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID 
    unsigned short count;	// bytes being written
    unsigned int   smo;		// smo containing data being written		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_PUTS

struct stdfss_puts
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID 
    unsigned short count;	// bytes being written
    unsigned int   smo;		// smo containing data being written		
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_PUTC

struct stdfss_putc
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;	// file ID 
    unsigned short padding1;	
    unsigned int   c;		// smo containing data being written		
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

// define STDFSS_LINK

struct stdfss_link
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   smo_file;	// smo containing the path of the existing file
    unsigned int   smo_link;	// smo containing the path of the new link file
    unsigned short padding1;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_MKDIR

struct stdfss_mkdir
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   dir_path;	
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_FLUSH

struct stdfss_flush
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned short file_id;
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_CHANGEATT

struct stdfss_changeatt
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   file_path;	// smo with file path
    unsigned int   flags;		// new flags
    unsigned short substract;	// if 0 flags will be ored. if 1 flags will be removed.
    unsigned short ret_port;
}PACKED_ATT;

// STDFSS_DELETE

struct stdfss_delete
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   path_smo;	// file path
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_MKDEVICE

struct stdfss_mkdevice
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   path_smo;		// device file path
    unsigned int   service_name;	// service name
    unsigned short logic_deviceid;	// logic device id
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_FILEINFO

struct stdfss_fileinfo
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   path_smo;	// file path
    unsigned int   file_info_id; // indicates the id desired file info format.
    unsigned short file_info_smo; 
    unsigned short ret_port;
} PACKED_ATT;

// STDFSS_INIT

struct stdfss_init
{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   path_smo;	// root path to mount
    unsigned int   deviceid;	// id of the service device driver
    unsigned short logic_deviceid;
    unsigned short ret_port;
} PACKED_ATT;

// FILE INFO STRUCTURE

#define FILEINFO_ID 0x00000001

#define STDFSS_FILETYPE_DEVICE		0
#define STDFSS_FILETYPE_FILE		1
#define STDFSS_FILETYPE_DIRECTORY	2

#define STDFSS_ATT_READONLY	4
#define STDFSS_ATT_HIDDEN	2
#define STDFSS_ATT_ENCRYPTED	1

struct file_info0
{
    int file_type;
    unsigned long long creation_date;
    unsigned long long modification_date;
    unsigned long long file_size;
    int flags;
    int dir_entries; 
    int protection;     /* protection */
    int hard_links;
    int owner_id;
    int owner_group_id;

    int device_service_id;
    int logic_device_id;
};

/* TAKEOVER */

struct stdfss_takeover{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   file_id;
    unsigned int   mode_mask;   // accepted open modes for the file
    unsigned short task_id;
    unsigned short ret_port;
    /* The port for the response message */
} PACKED_ATT;

struct stdfss_return{
    unsigned char command;		/* Command to execute */
    unsigned short thr_id;		// support for multithreading for iolib
    unsigned char padding0;
    unsigned int   file_id;
    unsigned int   padding1;
    unsigned short padding2;
    unsigned short ret_port;
    /* The port for the response message */
} PACKED_ATT;

#endif

