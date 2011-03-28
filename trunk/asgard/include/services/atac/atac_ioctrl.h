/*
*
*	Functions defined on this file, allows the driver to see a a single disk as a set of
*	logic devices (for disks this would read partitions and export them as multiple disks)
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
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

#ifndef _ATACIOCTRLH_
#define _ATACIOCTRLH_

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

/* Ata controler IO ctrl commands. */

#define ATAC_IOCTRL_FINDLDEV	1
#define ATAC_IOCTRL_REMOVELDEV	2
#define ATAC_IOCTRL_CREATELDEV	3
#define ATAC_IOCTRL_ENUMDEVS	4

/* device errors */
#define ATAC_IOCTRLERR_OK				0
#define ATAC_IOCTRLERR_INVALIDSMO		1
#define ATAC_IOCTRLERR_NOTFOUND			2
#define ATAC_IOCTRLERR_ERR				3
#define ATAC_IOCTRLERR_DEVICEINUSE		4
#define ATAC_IOCTRLERR_ENUMFINISHED		5
#define ATAC_IOCTRLERR_XOWNERSHIP_REQUIRED	6


/* IOCTL Command for ATAC_IOCTRL_FINDLDEV */
struct atac_ioctl_finddev{
	unsigned int command;				// STDDEV_IOCTL
		/* Command to execute */		
	unsigned short request;				// ATAC_IOCTRL_FINDLDEV
	unsigned short logic_deviceid;		// the logic device on which IO will be issued (must be a physical device)
	unsigned int find_dev_smo;		// R/W smo to a find_dev_param structure.
	unsigned short msg_id;		
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;


struct atac_find_dev_param
{
	unsigned char ptype;		// partition type being sought
	int bootable;				// 1 = return partition which is bootable, 0 otherwise.
	char strtype[256];			// if partition type is 0x7F, this field will be used as the fs name for de ada os specification.

	unsigned int start_lba;			// starting lba for the partition.
	unsigned int size;				// partition size.
	unsigned int metadata_end;
	unsigned short ldevid;	// service will write result value here. if 0xFFFF device was not found.
} PACKED_ATT;

/* IOCTL Command for ATAC_IOCTRL_REMOVELDEV */
/*
	NOTE: In order to execute this command, the logic device must have no owners at all.
*/
struct atac_ioctl_removeldev{
	unsigned int command;				// STDDEV_IOCTL
		/* Command to execute */		
	unsigned short request;				// ATAC_IOCTRL_REMOVELDEV
	unsigned short logic_deviceid;		// the logic device on which IO will be issued (cannot be a physical device)
	unsigned int padding;				// this command won't take parameters
	unsigned short msg_id;		
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

/* IOCTL Command for ATAC_IOCTRL_CREATELDEV */
struct atac_ioctl_createldev{
	unsigned int command;				// STDDEV_IOCTL
		/* Command to execute */		
	unsigned short request;				// ATAC_IOCTRL_CREATELDEV
	unsigned short logic_deviceid;		// the logic device on which IO will be issued (must be a physical device)
	unsigned int create_dev_smo;		// R/W smo to a create_ldev_param structure.
	unsigned short msg_id;		
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;


struct atac_create_ldev_param
{
	unsigned char ptype;			// partition type for the logic device
	int bootable;					// 1 = return partition which is bootable, 0 otherwise.
	char strtype[256];				// if partition type is 0x7F, this field will be used as the fs name for de ada os specification.
	unsigned int start_lba;			// starting lba for the partition.
	unsigned int size;				// desired partition size in sectors. (NOTE: As it is implemented, this could actually become be size - 2*512)
									// NOTE2: The service won't check a partition fits on the disk. It will check if it collides with
									// a current partition.
	unsigned short new_id;			// this field will be completed by the service.
} PACKED_ATT;

/* IOCTL Command for ATAC_IOCTRL_ENUMDEVS */

struct atac_ioctl_enumdevs{
	unsigned int command;				// STDDEV_IOCTL
		/* Command to execute */		
	unsigned short request;				// ATAC_IOCTRL_ENUMDEVS
	unsigned short logic_deviceid;		// ignored.
	unsigned int enum_dev_smo;		// smo to a enum_dev_param structure.
	unsigned short msg_id;		
	unsigned short ret_port;
		/* The port for the response message */
} PACKED_ATT;

struct atac_enum_dev_info
{
	unsigned short id;				// device id.
	unsigned char ptype;			// partition type for the logic device
	int bootable;					// 1 = return partition which is bootable, 0 otherwise.
	unsigned int start_lba;			// starting lba for the partition.
	unsigned int size;				// partition size.
	unsigned int metadata_end;
	unsigned short pid;				// physical device id on which its located
    char strtype[256];				// if partition type is 0x7F, this field will be used as the fs name for de alt os specification.
} PACKED_ATT;

struct atac_enum_dev_param
{
	unsigned int continuation;			// must be set to 0 upon the first call. Then it must be left untouched.
										// this will be used on succesive calls to the IOCTRL command in order
										// to indicate the service which device should be written next onto the smo.
	struct atac_enum_dev_info devinf;	// Logic device information for the device corresponding to the 
										// continuation value.
} PACKED_ATT;

#endif

