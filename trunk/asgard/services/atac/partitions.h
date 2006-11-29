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

#ifndef _ATACPARTITIONSH_
#define _ATACPARTITIONSH_

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

struct partition_entry
{
	unsigned char bootable;		// 0x80 yes, 0x00 no.
	unsigned char start_CHS[3];
	unsigned char type;
	unsigned char ending_CHS[3];
	unsigned int lba;
	unsigned int size;
} PACKED_ATT;

struct partition_table_sector
{
	unsigned char padding[446]; // pad sector until 0x1BE
	struct partition_entry entries[4];
	unsigned short signature;	// found on the MBR 0xAA55
} PACKED_ATT;

struct ptinfo
{
	unsigned int lba;			// possition on disk at which the partition table is located
	struct partition_entry entries[4];
};

#endif

