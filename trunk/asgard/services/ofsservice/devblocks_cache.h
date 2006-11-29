/*
*
*	OFS (Obsession File system) Multithreaded implementation
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

#include "ofs_internals.h"

#ifndef DEVBLOCKSCHACHEH
#define DEVBLOCKSCHACHEH

#define OFS_DEVCACHE_MAXBLOCKS 8 * 10 // 40kb
#define OFS_DEVCACHE_INITIALHITS	3

struct bc_block
{
	char data[OFS_BLOCKDEV_BLOCKSIZE];	// block data
	unsigned int hits;			// hits
	unsigned int parser;			// was it cached by the parser?
	unsigned int dirty;
	int free_next;
};

struct bc_entry
{
	unsigned int lba;			// block lba
	unsigned int index;
	unsigned int deviceid;
	unsigned int ldeviceid;
};

struct bc_cache
{
	int free_first;
	struct bc_block blocks[OFS_DEVCACHE_MAXBLOCKS];			// this list will be kept ordered
	struct bc_entry order[OFS_DEVCACHE_MAXBLOCKS];			// ordered index list for blocks
	unsigned int buffer_count;
};

void bc_init();
void bc_insert(unsigned int dev, unsigned int ldev, char *buffer, unsigned int start_lba, unsigned int count, int parser);
void bc_update(struct smount_info *minf, char *buffer, unsigned int start_lba, unsigned int count, int parser);
void bc_invalidate_filebuffer(unsigned int dev, unsigned int ldev, unsigned int lba);
unsigned int bc_get(unsigned int dev, unsigned int ldev, char *buffer, unsigned int start_lba, unsigned int count);
unsigned int bc_getblock(struct smount_info *minf, unsigned int lba);

void bc_movedown(int index);
void bc_moveup(int index);
int bc_freeblock();
int bc_makeroom(unsigned int dev, unsigned int ldev, unsigned int ignorefrom, unsigned int ignoreto);



#endif


