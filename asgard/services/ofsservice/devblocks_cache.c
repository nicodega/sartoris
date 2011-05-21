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
#include "devblocks_cache.h"

struct mutex bcache_mutex;

/*
	The blocks cache will provide a mechanism for caching block devices read or written 
	to or from a block device. This mechanism is provided only for the parser. 
	File buffers have their own caching system, but this will update cached file buffers
	on writes. As directories can't be written by tasks this is a safe, though not nice
	policy.
*/
int bc_read(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, int parser, struct stdfss_res **ret )
{
	struct stdblockdev_readm_cmd breadmcmd;
	struct stdblockdev_res device_res;

	// Upon reads we will do the following (items marked with [P] are only performed on parser calls):
	//	- Fill buffer with already cached info.
	//	- Read blocks
	//	- Update cache. 
#ifdef OFS_DEVBLOCK_CACHE
	unsigned int count = buffer_size / OFS_BLOCKDEV_BLOCKSIZE;
	unsigned int glba = bc_get(minf->deviceid, minf->logic_deviceid, buffer, lba, count);

	if((glba - lba) == count)
	{
		return TRUE; // everything was cached
	}

	breadmcmd.buffer_smo = share_mem(minf->deviceid, buffer + (glba - lba) * OFS_BLOCKDEV_BLOCKSIZE, buffer_size - (glba - lba) * OFS_BLOCKDEV_BLOCKSIZE, WRITE_PERM);
	breadmcmd.pos = glba; 
	breadmcmd.count = count - (glba - lba);
#else 
	breadmcmd.buffer_smo = share_mem(minf->deviceid, buffer, buffer_size, WRITE_PERM);
	breadmcmd.pos = lba; 
	breadmcmd.count = buffer_size / OFS_BLOCKDEV_BLOCKSIZE;
#endif

	// startd dir buffer filling on first block
	breadmcmd.command = BLOCK_STDDEV_READM;
	breadmcmd.dev = minf->logic_deviceid;
	breadmcmd.msg_id = wpid;
	breadmcmd.ret_port = OFS_BLOCKDEV_PORT;

	// send STDBLOCKDEV_READ for first part of the root dir file
	locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &breadmcmd, wpid);

	claim_mem(breadmcmd.buffer_smo);

	get_signal_msg((int *)&device_res, wpid);

	if(device_res.ret == STDBLOCKDEV_ERR)
	{
		*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
		return FALSE;
	}

#ifdef OFS_DEVBLOCK_CACHE
	bc_insert(minf->deviceid, minf->logic_deviceid, buffer, lba, count, parser);
#endif
	return TRUE;
}

int bc_write(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, int parser, struct stdfss_res **ret )
{
	struct stdblockdev_writem_cmd bwritemcmd;
	struct stdblockdev_res device_res;

	// Upon writes we will do the following (items marked with [P] are only performed on parser calls):
	//	- Write the block/s
	//	- [P] Update blocks cache.
	//	- [P] Update filebuffers.
	// (This caching system will work in a write through way)

	// startd dir buffer filling on first block
	bwritemcmd.buffer_smo = share_mem(minf->deviceid, buffer, buffer_size, READ_PERM);
	bwritemcmd.command = BLOCK_STDDEV_WRITEM;
	bwritemcmd.dev = minf->logic_deviceid;
	bwritemcmd.msg_id = wpid;
	bwritemcmd.pos = lba;
	bwritemcmd.ret_port = OFS_BLOCKDEV_PORT;
	bwritemcmd.count = buffer_size / OFS_BLOCKDEV_BLOCKSIZE;

	// send STDBLOCKDEV_READ for first part of the root dir file
	locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &bwritemcmd, wpid);
	
	claim_mem(bwritemcmd.buffer_smo);

	get_signal_msg((int *)&device_res, wpid);

	if(device_res.ret == STDBLOCKDEV_ERR)
	{
		*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
		return FALSE;
	}

#ifdef OFS_DEVBLOCK_CACHE
	bc_update(minf, buffer, lba, buffer_size / OFS_BLOCKDEV_BLOCKSIZE, parser);
#endif

	return TRUE;
}

/* Cache implementation */


struct bc_cache bcache;

void bc_init()
{
	init_mutex(&bcache_mutex);
	
	bcache.buffer_count = 0;
	bcache.free_first = 0;
	int i = 0;

	while(i < OFS_DEVCACHE_MAXBLOCKS)
	{
		bcache.order[i].lba = (unsigned int)-1;
		bcache.order[i].index = -1;
		bcache.blocks[i].free_next = ((i+1 == OFS_DEVCACHE_MAXBLOCKS)? -1 : i+1);
		i++;
	}
}

/*returns: > 0 greater, 0 equal, < 0 lower*/
int bc_compare(struct bc_entry *e1, struct bc_entry *e2)
{
	/* ordered by deviceid, ldeviceid, lba */
	if(e1->deviceid != e2->deviceid) return e1->deviceid - e2->deviceid;
	if(e1->ldeviceid != e2->ldeviceid) return e1->ldeviceid - e2->ldeviceid;
	if(e1->lba != e2->lba) return e1->lba - e2->lba;
	return 0;
}

/* Find a block on the list. If not present will return last intended possition (i.e. where it should be inserted) */
int bc_getindex(struct bc_entry *e)
{
	// Binary search
	int low = 0, high = (int)bcache.buffer_count - 1;
	int pos = 0;

	int cmp = 0;

	while(low <= high)
	{
		pos = ((high + low) >> 1);
		cmp = bc_compare(e, &bcache.order[pos]);

		if(cmp > 0)
		{
			// sought item is greater than current item
			// move i to pos + 1
			low = pos+1;
		}
		else if(cmp < 0)
		{
			// sought item is lower tan current, move 
			// j to pos - 1;
			high = pos-1;
		}
		else
		{
			e->index = bcache.order[pos].index;
			return pos;
		}
	}

	if(cmp > 0)
	{
		pos++;
	}

	return pos;
}

/*
	It will use a hits system, and only parser updates
	will displace parser blocks.
*/
void bc_insert(unsigned int dev, unsigned int ldev, char *buffer, unsigned int start_lba, unsigned int count, int parser)
{
	// we will insert in an ordered fashion. Insert will be called upon reading.
	// For each block we will:
	// 	- If the block exists we will do nothing
	// 	- If the block does not exists 
	//		- if parser is FALSE we insert as many blocks as there is free space on the cache.
	//		- if parser is TRUE we get enough space on the cache, and insert the blocks.
	struct bc_entry e;

	/* FIXME: This algorithm could be implemented more efficiently */

	/* Allocate buffers */	
	unsigned int off = 0;

	e.deviceid = dev;
	e.ldeviceid = ldev; 
	e.lba = start_lba;

	wait_mutex(&bcache_mutex);

	while(count > 0)
	{
		e.index = -1;	

		int start = bc_getindex(&e);

		if(e.index != -1)
		{
			// block exists, 
			count--;
			off += OFS_BLOCKDEV_BLOCKSIZE;
			e.lba++;
			continue;
		}
		else if (!parser && bcache.buffer_count == OFS_DEVCACHE_MAXBLOCKS)
		{
			// block does not exists, there is no room, and it's not a parser request.
			leave_mutex(&bcache_mutex);
			return;
		}

		// allocate a block
		if(bcache.buffer_count == OFS_DEVCACHE_MAXBLOCKS)
		{
			// must deallocate first, for there is no room on cache
			e.index = bc_makeroom(dev, ldev, start_lba, start_lba + count - 1);

			// make room could have moved order up on 1
			if(bc_compare(&e, &bcache.order[start]) < 0)
			{
				start--;
			}
		}
		else
		{
			// set the index
			e.index = bc_freeblock();
		}
		// insert on the order
		bc_moveup(start); // send bigger elements one index up

		bcache.order[start] = e;
		bcache.blocks[e.index].parser = parser;
		bcache.blocks[e.index].hits = OFS_DEVCACHE_INITIALHITS;
		bcache.blocks[e.index].dirty = FALSE;

		// copy block contents
		mem_copy((unsigned char*)buffer + off, (unsigned char*)bcache.blocks[e.index].data, OFS_BLOCKDEV_BLOCKSIZE);

		count--;
		bcache.buffer_count++;
		off += OFS_BLOCKDEV_BLOCKSIZE;
		e.lba++;
	} 

	leave_mutex(&bcache_mutex);
}

void bc_moveup(int index)
{
	if(index > (int)bcache.buffer_count - 1) return;
	int i = bcache.buffer_count;
	while(i > index)
	{
		bcache.order[i] = bcache.order[i-1];
		i--;
	}
}

void bc_movedown(int index)
{
	while(index < bcache.buffer_count-2)
	{
		bcache.order[index] = bcache.order[index+1];
		index++;
	}
}

/* Returns a free block index */
int bc_freeblock()
{
	int ret = bcache.free_first;
	bcache.free_first = bcache.blocks[bcache.free_first].free_next;
	return ret;
}

/* Make room on our cache for count blocks */
/* This will free buffers, and later re arrange the order */
int bc_makeroom(unsigned int dev, unsigned int ldev, unsigned int ignorefrom, unsigned int ignoreto)
{
	// find our candidate for removal
	unsigned int chits = (unsigned int)-1;
	unsigned int candidate = -1;
	int cparser = 0;
	int index = 0;

	while(index < OFS_DEVCACHE_MAXBLOCKS)
	{
		if(bcache.order[index].lba < ignorefrom || bcache.order[index].lba > ignoreto)
		{
			if(bcache.blocks[bcache.order[index].index].hits < chits || (cparser && !bcache.blocks[bcache.order[index].index].parser)) 
			{
				chits = bcache.blocks[bcache.order[index].index].hits;
				candidate = bcache.order[index].index;
				cparser = bcache.blocks[bcache.order[index].index].parser;
			}
		}
		index++;
	}

	bcache.buffer_count--;
	bcache.blocks[candidate].dirty = FALSE;
	bcache.blocks[candidate].free_next = -1;
	bcache.free_first = candidate;
	bc_movedown(candidate);
}

/* 	This function will update the cache with buffer blocks. 
*/
void bc_update(struct smount_info *minf, char *buffer, unsigned int start_lba, unsigned int count, int parser)
{
	struct bc_entry e;
	e.deviceid = minf->deviceid;
	e.ldeviceid = minf->logic_deviceid; 
	e.lba = start_lba;
	e.index = -1;

	wait_mutex(&bcache_mutex);

	int start; int end;
	// find boundaries
	unsigned int c = count;
	while(c > 0)
	{
		start = bc_getindex(&e);
		if(e.index != -1) break;
		e.lba++;
		c--;		
	}

	if(start_lba + count == e.lba)
	{
		leave_mutex(&bcache_mutex);
		return; // none found
	}

	e.lba = start_lba;

	end = start;
	
	while(end < bcache.buffer_count && (end - start) < c)
	{
		if(bcache.order[start].deviceid != bcache.order[end].deviceid || bcache.order[start].ldeviceid != bcache.order[end].ldeviceid || bcache.order[start].lba != e.lba) break;
		e.lba++;
		end++;
	}
	end--;

	unsigned int lastinv = 0;
	unsigned int lba = start_lba;
	int i = 0;
	// and update buffers
	while(start <= end)
	{
		// update block
		mem_copy((unsigned char*)buffer + i * OFS_BLOCKDEV_BLOCKSIZE, (unsigned char*)bcache.blocks[bcache.order[start].index].data, OFS_BLOCKDEV_BLOCKSIZE);

		bcache.blocks[bcache.order[start].index].dirty = TRUE;
		if(bcache.blocks[bcache.order[start].index].hits + 1 != (unsigned int)-1) bcache.blocks[bcache.order[start].index].hits++;

		// invalidate filebuffers if needed
		// get OFS block for the lba
		unsigned int block = bc_getblock(minf, lba);

		if(lastinv != block && block != -1)
		{
			lastinv = block;
			bc_invalidate_filebuffer(e.deviceid, e.ldeviceid, block);
		}
		start++;
		lba++;
	}
	leave_mutex(&bcache_mutex);
}

unsigned int bc_getblock(struct smount_info *minf, unsigned int lba)
{
	// we have to calculate block displacement
	int i = 0;
	while(i < minf->ofst.group_count)
	{
		if(minf->group_headers[i].blocks_offset <= lba && lba < minf->group_headers[i].blocks_offset + OFS_BLOCKDEVBLOCK_SIZE * minf->ofst.blocks_per_group)
		{
			return (unsigned int)((lba - minf->group_headers[i].blocks_offset) / OFS_BLOCKDEVBLOCK_SIZE) + minf->group_headers[i].blocks_offset;
		}
		i++;
	}
	return -1;
}

/* This function will be called from fb_get tu update cached filebuffers */
void bc_invalidate_filebuffer(unsigned int dev, unsigned int ldev, unsigned int block_lba)
{
	struct file_buffer *buffer = NULL;

	int i = 0;
	while(i < OFS_FILEBUFFERS)
	{
		if(file_buffers[i].block_lba == block_lba)
		{
			buffer = &file_buffers[i];
			break;
		}		
		i++;
	}

	if(buffer == NULL) return;
//print("Buffer will be invalidated", block_lba);
	// we should invalidate a file buffer here
	// what it will do is tell the filebuffers subsystem
	// that buffer is out of date.
	// Hence, when fb_get is issued, it'll fill it again.
	wait_mutex(&buffer->invalidated_mutex);
	buffer->invalidated = TRUE;
	leave_mutex(&buffer->invalidated_mutex);
}

/* Will fill the buffer with cached blocks, until the first non cached block is found 
   It will return the lba of the first non cached block. */
unsigned int bc_get(unsigned int dev, unsigned int ldev, char *buffer, unsigned int start_lba, unsigned int count)
{
	struct bc_entry e;
	e.deviceid = dev;
	e.ldeviceid = ldev; 
	e.lba = start_lba;
	e.index = -1;

	wait_mutex(&bcache_mutex);
	
	int start = bc_getindex(&e);
	unsigned int end = start_lba;

	if(e.index != -1)
	{
		// now find the end
		end = start;

		while(end < bcache.buffer_count && (end - start) < count)
		{
			if(bcache.order[start].deviceid != bcache.order[end].deviceid || bcache.order[start].ldeviceid != bcache.order[end].ldeviceid || bcache.order[end].lba != e.lba) break;
			e.lba++;
			end++;
		}
		unsigned int fcount = end - start;
		int i = 0;
		while(start < end)
		{
			// copy block
			mem_copy((unsigned char*)bcache.blocks[bcache.order[start].index].data, (unsigned char*)buffer + i*OFS_BLOCKDEV_BLOCKSIZE, OFS_BLOCKDEV_BLOCKSIZE);
	
			start++;
			i++;
		}
		leave_mutex(&bcache_mutex);	
		return start_lba + fcount;
	}
	else
	{
		leave_mutex(&bcache_mutex);
		return start_lba;
	}
}


