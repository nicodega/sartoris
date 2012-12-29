/*
*	Process and Memory Manager Service
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

#include "../types.h"

#ifndef INITFS2H
#define INITFS2H

#define MAX_NAME_SIZE 15

/*
Init FS version 2:
	- Contents can be compressed using LZW algorithm
	- Services being loaded are identified on the fs Header

	Init image will have this structure:
	Header | Service Descriptor1 .... Service Descriptor1 | Image1 ... ImageN
*/

#define IFS2MAGIC       0x5A270215	// use your imagination and guess what's written on the magic number

// Init image will have this structure:
//
// Init | InitHeader | Header1 ... HeaderN | Image1 ... ImageN
struct ifs2_header
{
	unsigned int ifs2magic;	// must be 0x5A270215
	unsigned int flags;
	unsigned int size;		// complete fs size
	unsigned int entries;	// ammount of entries on initfs
	unsigned int headers;	// headers offset from begining of ifs
} PACKEDATT;

#define IFS2_FLAG_NONE  0
#define IFS2_FLAG_LZW   1  // compressed contents (9 - 12 bit compression)

/* Service descriptor */
struct ifs2srv_header
{
	unsigned int image_size;	  // size of the image (without the header size)
	unsigned int image_pos;		  // image location from the begining of images
	unsigned short flags;		  // flags
	unsigned short pman_type;	  // service type (used and defined by pmanager)
	unsigned short task;		  // desired task id (if 0xFFFF, it will be ignored)
	unsigned short main_thread;   // desired main thread id (if 0xFFFF, it will be ignored)
	char img_name[MAX_NAME_SIZE]; // 0 terminated string with image name
} PACKED_ATT;

#define IFS2SRV_FLAG_NONE       0
#define IFS2SRV_FLAG_LOWMEM     1  // Must be loaded on low memory
#define IFS2SRV_FLAG_IGNORE		2  // PMAN should not load this service (used for inhibiting services to debug PMAN)

#define IFS2SRV_PMTYPE_NORMAL     0		// nothing to care
#define IFS2SRV_PMTYPE_MAINFS     1		// this is the main FS service (or fs hub whatever)
#define IFS2SRV_PMTYPE_HDD        2		// main HDD controller (or controller hub, whatever)
#define IFS2SRV_PMTYPE_DYNLINK    3		// OS dynamic linker


#endif
