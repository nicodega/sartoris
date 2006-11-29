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

#ifndef _PCIH_
#define _PCIH_

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif


#define PCI_UIID	0x00000009
#define PCI_VER		0x1

/* Get a list of memory regions used by mapped devices */
#define PCI_GETMAPPED_REGIONS	1
/* Get configuration for an specific device */
#define PCI_GETCONFIG			2
/* Set configuration for a device */
#define PCI_CONFIGURE			3
/* Set configuration for a device */
#define PCI_GETDEVICES			4

/* Values for the ret field on pci_res */
#define PCIERR_OK					0
#define PCIERR_DEVICE_NOT_FOUND		1
#define PCIERR_INVALID_CONTINUATION	2
#define PCIERR_INVALID_COMMAND		4
#define PCIERR_SMO_TOO_SMALL		5
#define PCIERR_NOTIMPLEMENTED		6
#define PCIERR_UNKNOWN				7

/* Generic command format */
struct pci_cmd
{
	short command;
	short thr_id;
	int specific0;
	int specific1;
	short specific2;
	char specific3;
	char ret_port;
} PACKED_ATT;

/* Generic response format */
struct pci_res
{
	short command;
	short thr_id;
	int specific0;
	int specific1;
	short ret;
	short ret_port;
} PACKED_ATT;

/* PCI_GETMAPPED_REGIONS */
/* This message will return a continuation value on specific0.
The first message must be issued with continuation set to 0.
of it's ret msg. It will also return written bytes on specific1. */
struct pci_getmregions
{
	short command;
	short thr_id;
	int buffer_smo;
	int continuation;
	short specific2;
	char specific3;
	char ret_port;
} PACKED_ATT;

/* PCI_GETCONFIG */
struct pci_getcfg
{
	short command;
	short thr_id;
	char class;	
	short sclass;
	short vendorid;		/* vendor id */
	char function;		/* 1...8 */
	char progif;
	short cfg_smo; 
	char ret_port;
} PACKED_ATT;

/* PCI_SETCONFIG */
struct pci_setcfg
{
	short command;
	short thr_id;
	char class;	
	short sclass;
	short vendorid;		/* vendor id */
	char function;		/* 1...8 */
	char progif;
	short cfg_smo; 
	char ret_port;
} PACKED_ATT;

/* PCI_GETDEVICES */
/* This message will return a continuation value on specific0.
The first message must be issued with continuation set to 0.
of it's ret msg. It will also return written bytes on specific1. */
struct pci_getdevices
{
	short command;
	short thr_id;
	int buffer_smo;	// minimu
	int continuation;
	short specific2;
	char specific3;
	char ret_port;
} PACKED_ATT;

/* Structure copied onto pci_getdevices buffer. */
struct pci_dev_info
{
	char class;			/* class */
	short sclass;		/* sub class */
	short vendorid;		/* vendor id */
	char function;		/* 1...8 */
	char progif;
} PACKED_ATT;

/* Structures used for memory regions */
struct mem_region
{
	unsigned int size;	// size of this structure
	unsigned int start;
	unsigned int end;
} PACKED_ATT;

/* Structure used for get/set config commands */
struct pci_cfg
{
	unsigned short status;
	unsigned short command;
	unsigned short class_sclass;
	unsigned char progif;
	unsigned char revision;
	unsigned char self_test;
	unsigned char header;
	unsigned char latency;
	unsigned char cache_line_size;

	unsigned int baseaddrr0;
	unsigned int baseaddrr1;
	unsigned int baseaddrr2;
	unsigned int baseaddrr3;
	unsigned int baseaddrr4;
	unsigned int baseaddrr5;

	unsigned int cardbus_CIS;

	unsigned short subsystemid;
	unsigned short subsystemvendorid;

	unsigned int expromaddr;

	unsigned int res0;
	unsigned int res1;

	unsigned char max_latency;
	unsigned char min_grant;
	unsigned char interupt_Pin;
	unsigned char irq_Line;
} PACKED_ATT;


#endif

