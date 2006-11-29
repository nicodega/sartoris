/*
*   	OBSESSION FILE SYSTEM STRUCTURES
*
*	This file defines OFS structures.
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

#ifndef OFSH
#define OFSH


// std protection modes
#define OFS_PROTECTIONMODE_OURWX	00700 // user (file owner)  has  read,  write and execute permission
#define OFS_PROTECTIONMODE_OUR		00400 // user has read permission
#define OFS_PROTECTIONMODE_OUW		00200 // user has write permission
#define OFS_PROTECTIONMODE_OUX		00100 // user has execute permission
#define OFS_PROTECTIONMODE_OGRWX	00070 // group (file owner group)  has  read,  write and execute permission
#define OFS_PROTECTIONMODE_OGR		00040 // group has read permission
#define OFS_PROTECTIONMODE_OGW		00020 // group has write permission
#define OFS_PROTECTIONMODE_OGX		00010 // group has execute permission
#define OFS_PROTECTIONMODE_OTRWX	00007 // others has  read,  write and execute permission
#define OFS_PROTECTIONMODE_OTR		00004 // others has read permission
#define OFS_PROTECTIONMODE_OTW		00002 // others has write permission
#define OFS_PROTECTIONMODE_OTX		00001 // others has execute permission

	
// OFS disk layout defines //

#define OFS_BLOCKDEV_BLOCKSIZE 512

#define OFS_MAGIC_NUMBER 0xffdb		// the magic number :-)
#define OFS_NODESPER_BLOCKDEV_BLOCK (int)(OFS_BLOCKDEV_BLOCKSIZE / sizeof(struct node))
#define OFS_BLOCKDEVBLOCK_SIZE 8 // on BLOCKDEV units
#define OFS_BLOCK_SIZE  4096 //OFS_BLOCKDEVBLOCK_SIZE * OFS_BLOCKDEV_BLOCKSIZE // BLOCK SIZE IN BYTES
#define BITS_PER_DEVBLOCK OFS_BLOCKDEV_BLOCKSIZE * 8

// NOTE: OFS_BLOCK_SIZE is 4096 and not OFS_BLOCKDEVBLOCK_SIZE * OFS_BLOCKDEV_BLOCKSIZE because
// when debugging operations of % turned out to fail calculations with unsigned long long
// this is probably compiler specific

	/////////// FILE SYSTEM ///////////
#define OFS_NULL_VALUE -1

// node types

//xxxxxxxx xxxxxxxx xxxxxxxx xxx|read only|props|dev|dir|file

#define OFS_CHILD			0x10	// a child node of a file
#define OFS_PROPERTIES_FILE 0x8		// a system property file
#define OFS_DEVICE_FILE		0x4		// a device file	
#define OFS_DIRECTORY_FILE	0x2		// a directory
#define OFS_FILE			0x1		// a file base node

#define PTRS_ON_NODE 20		// this should give a 128 bytes node size (node size must be a power of 2) //

// directory flags
#define OFS_NON_MOVABLE_FILE	0x8
#define OFS_HIDDEN_FILE			0x4
#define OFS_READ_ONLY_FILE		0x2
#define OFS_DELETED_FILE		0x1
#define OFS_NOFLAGS			0x0

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

// File system table //
struct OFST{
	unsigned int mount_count;
	     /*Indicates how many times the disk has been mounted.
	     (This could be used to detect possible errors at boot time)*/
	unsigned int block_size;
	     /*Size of the blocks used on the groups.*/
	unsigned int node_size;
		 /* size of nodes (must be a power of 2)*/
	unsigned int first_group;
	     /*Offset of the first group on the partition.*/
	unsigned int ptrs_on_node;
	unsigned int blocks_per_group;
	unsigned int nodes_per_group;
	unsigned int group_count;
	     /*Nº of groups on the device.*/
	unsigned int MetaData;
	     /*The OFST structure may contain an options table following
	     this structure. This variable indicated the ID or TYPE of
	     this Options structure. If 0 it means no Options are present.
	     (could be used for permissions, encryption, etc)*/
	unsigned int Magic_number;
	     /*just to keep up with tradition :-)*/
} PACKED_ATT;

// group header //
struct GH{
	unsigned int group_id;
		/*The position of the group. (0..GROUPS_COUNT-1)*/
	unsigned int blocks_per_group; // same as OFST
	unsigned int nodes_per_group; // same as OFST
	unsigned int nodes_bitmap_offset;
	unsigned int blocks_bitmap_offset;
	unsigned int nodes_table_offset;
	unsigned int blocks_offset;
	unsigned int group_size;	// same on all groups
	unsigned int meta_data_size;
	unsigned int meta_data;
	    /*If zero, it means there are no options. If nonzero it identifies
		the options structure that follows.*/
} PACKED_ATT;

// nodes //
struct node{
	unsigned int type;
	     /*This field will indicate whether the node identifies a file, a dir or
	     anything else...
	     (props files shouldn't be treated as normal files)*/
	unsigned long long file_size;
	     /*in bytes;*/
	unsigned int link_count;
		 /* used for garbage collection */
	unsigned long long creation_date;
	unsigned long long modification_date;
	unsigned int owner_id;
	unsigned int owner_group_id;
	unsigned int protection_mode;
	unsigned int blocks[PTRS_ON_NODE];
	     /*LBA address of blocks of the node.
	     (NOTE: blocks could be on other groups)*/
	unsigned int next_node;
	     /*if the file/dir/.. has more blocks than PTRS_ON_NODE this will be nonzero
	     and will contain the LBA address of the next node. */
} PACKED_ATT;


// dir entries //
struct sdir_entry
{
	unsigned int record_length;
	unsigned int nodeid;
	unsigned int name_length;
	unsigned int flags;
} PACKED_ATT;

// device files //
struct sdevice_file
{
	unsigned int logic_deviceid;	
	unsigned int service_name_size;
} PACKED_ATT;


#endif
