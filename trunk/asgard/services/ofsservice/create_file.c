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

// Create file v2.0 (using file buffers, so that there are no conflicts with dir readings, or fb caching)

#include "ofs_internals.h"

// This will create a file. If out_newnode is NULL, out_nodeid MUST be non-null and a link will be created.
int create_file(struct gc_node *directory_node, char *file_name, int file_name_start, int node_type, int force_preservation, struct smount_info *minf, int wpid, int command, unsigned int *out_nodeid, struct gc_node **out_newnode, int flags, struct stdfss_res **ret)
{
	struct gc_node *dir_node = NULL;				// current directory node
	unsigned int *free_nodeid = NULL;				// a pointer to our new node id
	unsigned int *free_blockid = NULL;
	int linking = out_newnode == NULL;
	unsigned char dentryb[sizeof(struct sdir_entry)];		// a buffer were we will map our dir entry
	struct sdir_entry *dentry = (struct sdir_entry *)dentryb;	// ehm... this is cheating ^^
	int i = 0;

	if(!linking)
	{
		// create gc_node
		*out_newnode = (struct gc_node*)malloc(sizeof(struct gc_node));
		(*out_newnode)->ref = 1;

		i = 0;
		// initialize new node
		while(i < PTRS_ON_NODE)
		{
			(*out_newnode)->n.blocks[i] = OFS_NULL_VALUE;
			i++;
		}

		(*out_newnode)->n.file_size = 0;
		(*out_newnode)->n.creation_date = 666; // TODO SET FOR TODAY
		(*out_newnode)->n.modification_date = 0;
		(*out_newnode)->n.link_count = 1;
		(*out_newnode)->n.next_node = OFS_NULL_VALUE;
		(*out_newnode)->n.type = node_type;
		(*out_newnode)->n.owner_group_id = 0;
		(*out_newnode)->n.owner_id = 0;
		(*out_newnode)->n.protection_mode = OFS_PROTECTIONMODE_OUR | OFS_PROTECTIONMODE_OGR | OFS_PROTECTIONMODE_OTR;

		// get a free node
		free_nodeid = get_free_nodes(1, force_preservation, minf, command, wpid, ret);

		if(free_nodeid == NULL){ free(*out_newnode); return FALSE; }

		(*out_newnode)->nodeid = *free_nodeid;

		// write node to disk
		if(!write_node(*out_newnode, minf, wpid, command, ret))
		{
			// free the aquired node
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
			free(*out_newnode);

			return FALSE;
		}
	}

	// Lock the directory node to avoid parsing until we are finished
	// NOTE: we don't lock the node for it has no entry on 
	// any directory and it's no longer available
	
	// lock dir for inserting the entry
	if(!lock_node(directory_node->nodeid, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_DIRECTORY | OFS_NODELOCK_BLOCKING, OFS_LOCKSTATUS_DENYALL, wpid, command, ret))
	{
		// free the aquired node
		if(free_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
		}
		if(*out_newnode != NULL) free(*out_newnode);
		return FALSE;
	}

	// Check if we have room on the directory last node
	dir_node = nref(minf->dinf, directory_node->nodeid);
	unsigned long long dir_used_size = directory_node->n.file_size;

	while(dir_node->n.next_node != OFS_NULL_VALUE)
	{
		if(!read_node(&dir_node, dir_node->n.next_node, minf, wpid, command, ret))
		{
			// free the aquired node
			if(free_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
				free(free_nodeid);
			}
			nfree(minf->dinf, dir_node);
			if(*out_newnode != NULL) free(*out_newnode);
			*out_newnode = NULL;
			unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			return FALSE;
		}

		dir_used_size -= PTRS_ON_NODE * OFS_BLOCK_SIZE;
	}

	// Now dir_node contains the directory last node.
	// Calculate our current block, and current block used space
	unsigned int curr_block = 	(unsigned int)((unsigned int)dir_used_size / OFS_BLOCK_SIZE) 
					+ ((dir_used_size % OFS_BLOCK_SIZE == 0 && dir_used_size != 0)? 1 : 0);
	unsigned int curr_block_cursor = (unsigned int)dir_used_size % OFS_BLOCK_SIZE;

	int entrylen = len(file_name) - file_name_start + sizeof(struct sdir_entry) + 1;

	// We will support entries up to a block
	if(entrylen > OFS_BLOCK_SIZE)
	{
		*ret = build_response_msg(command, STDFSSERR_FILENAME_TOOLONG);
		if(free_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
		}
		nfree(minf->dinf, dir_node);
		if(*out_newnode != NULL) free(*out_newnode);
		*out_newnode = NULL;
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return FALSE;
	}

	// get a block if needed
	if( entrylen > OFS_BLOCK_SIZE - curr_block_cursor || dir_node->n.blocks[curr_block] == OFS_NULL_VALUE )
	{
		free_blockid = get_free_blocks(1, force_preservation, minf, command, wpid, ret);

		if(free_blockid == NULL)
		{
			if(free_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
				free(free_nodeid);
			}
			nfree(minf->dinf, dir_node);
			if(*out_newnode != NULL) free(*out_newnode);
			*out_newnode = NULL;
			unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			return FALSE;
		}
	}

	unsigned int *newdir_nodeid = NULL;
	struct gc_node *new_dirnode = NULL;

	// if a new node is needed get one
	if(entrylen > OFS_BLOCK_SIZE - curr_block_cursor && curr_block == PTRS_ON_NODE - 1)
	{
		// get a free node
		newdir_nodeid = get_free_nodes(1, force_preservation, minf, command, wpid, ret);

		if(newdir_nodeid == NULL)
		{
			if(free_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
				free(free_nodeid);
			}
			nfree(minf->dinf, dir_node);
			if(*out_newnode != NULL) free(*out_newnode);
			*out_newnode = NULL;
			if(free_blockid != NULL)
			{
				free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
				free(free_blockid);
			}
			unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			return FALSE;
		}

		new_dirnode = (struct gc_node*)malloc(sizeof(struct gc_node));
		new_dirnode->ref = 1;
		new_dirnode->nodeid = *newdir_nodeid;
		
		i = 0;
		while(i < PTRS_ON_NODE)
		{
			new_dirnode->n.blocks[i] = OFS_NULL_VALUE;
			i++;
		}

		new_dirnode->n.file_size = 0; 
		new_dirnode->n.creation_date = 0;
		new_dirnode->n.modification_date = 0;
		new_dirnode->n.link_count = 1;
		new_dirnode->n.type = OFS_CHILD;
		new_dirnode->n.protection_mode = 0;
		new_dirnode->n.owner_group_id = directory_node->n.owner_group_id;
		new_dirnode->n.owner_id = directory_node->n.owner_id;
		new_dirnode->n.next_node = OFS_NULL_VALUE;

		// Adding the node here to the references is safe, because we have dirnode locked on exclusive
		// and filesize will be modified later when finished.
		wait_mutex(&minf->dinf->nodes_mutex);
		avl_insert(&minf->dinf->nodes, new_dirnode, new_dirnode->nodeid);
		leave_mutex(&minf->dinf->nodes_mutex);
	}

	struct gc_node *curr_node = nref(minf->dinf, dir_node->nodeid);

	// select working node
	if(OFS_BLOCK_SIZE == curr_block_cursor && curr_block == PTRS_ON_NODE - 1)
	{
		nfree(minf->dinf, curr_node);
		// NOTE: The new dir node is already referenced
		curr_node = nref(minf->dinf, new_dirnode->nodeid);
		curr_block = 0;
		dir_node->n.next_node = *newdir_nodeid;
	}
	
	// if there is no block assign it now
	if(curr_node->n.blocks[curr_block] == OFS_NULL_VALUE)
	{
		// get a new block for the directoy
		curr_block_cursor = 0;
		curr_node->n.blocks[curr_block] = *free_blockid;
	}

	// setup dentry
	dentry->flags = flags;
	dentry->name_length = entrylen - sizeof(struct sdir_entry)-1;
	dentry->nodeid = ((linking)? *out_nodeid : *free_nodeid);
	dentry->record_length = entrylen;

	unsigned int written = 0;
	
	// NOTE: What I'll do is not nice, but I need to use a file buffer just in case
	// other process is reading from the directory file.
	// File buffers so far could only be assigned to stask_file_info structs, and it's better
	// to keep it that way. Hence, I'll create a file info struct now and get a buffer for it.
	struct stask_file_info *finf = (struct stask_file_info *)malloc(sizeof(struct stask_file_info));

	finf->dinf = minf->dinf;
	finf->deviceid = minf->deviceid;
	finf->logic_deviceid = minf->logic_deviceid;
	finf->current_node = curr_node;
	finf->assigned_buffer = NULL;
	finf->current_block = curr_block;

	// Notice the loop wont enter more than twice, because of the entry size restriction of
	// 1 OFS BLOCK
	int second_block = FALSE;
	while(written < entrylen)
	{
		// get the buffer
		if(!fb_get(wpid, command, finf, !second_block, ret))
		{
			if(free_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
				free(free_nodeid);
			}
			if(newdir_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
				free(newdir_nodeid);
			}
			if(new_dirnode != NULL)
			{
				nfree(minf->dinf, new_dirnode);
				dir_node->n.next_node = OFS_NULL_VALUE;
			}
			nfree(minf->dinf, curr_node);
			nfree(minf->dinf, dir_node);
			
			if(*out_newnode != NULL) free(*out_newnode);
			*out_newnode = NULL;
			if(free_blockid != NULL)
			{
				free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
				free(free_blockid);
			}
			
			fb_finish(wpid, finf);
			unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			return FALSE;
		}

		// write entry
		while(written < sizeof(struct sdir_entry) && curr_block_cursor < OFS_BLOCK_SIZE)
		{	
			finf->assigned_buffer->buffer[curr_block_cursor] = dentryb[written];
			written++;
			curr_block_cursor++;
		}

		// continue writing file_name
		while(written < entrylen && curr_block_cursor < OFS_BLOCK_SIZE)
		{	
			finf->assigned_buffer->buffer[curr_block_cursor] = file_name[written - sizeof(struct sdir_entry) + file_name_start];
			written++;
			curr_block_cursor++;
		}

		finf->assigned_buffer->dirty = TRUE;

		// preserve buffer changes
		if(!fb_write(wpid, finf, ret))
		{
			if(free_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
				free(free_nodeid);
			}
			if(newdir_nodeid != NULL)
			{
				free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
				free(newdir_nodeid);
			}
			if(new_dirnode != NULL)
			{
				nfree(minf->dinf, new_dirnode);
				dir_node->n.next_node = OFS_NULL_VALUE;
			}
			nfree(minf->dinf, curr_node);
			nfree(minf->dinf, dir_node);
			
			if(*out_newnode != NULL) free(*out_newnode);
			*out_newnode = NULL;
			if(free_blockid != NULL)
			{
				free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
				free(free_blockid);
			}
			
			fb_finish(wpid, finf);
			unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			return FALSE;
		}

		// if there is still something to be written and a new node exists, switch nodes
		if(written < entrylen && OFS_BLOCK_SIZE == curr_block_cursor && curr_block == PTRS_ON_NODE - 1)
		{
			// preserve current node if different from base
			if(directory_node->nodeid != dir_node->nodeid && !write_node(finf->current_node, minf, wpid, command, ret))
			{
				if(free_nodeid != NULL)
				{
					free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
					free(free_nodeid);
				}
				if(newdir_nodeid != NULL)
				{
					free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
					free(newdir_nodeid);
				}
				if(new_dirnode != NULL)
				{
					nfree(minf->dinf, new_dirnode);
					dir_node->n.next_node = OFS_NULL_VALUE;
				}
				nfree(minf->dinf, curr_node);
				nfree(minf->dinf, dir_node);
				
				if(*out_newnode != NULL) free(*out_newnode);
				*out_newnode = NULL;
				if(free_blockid != NULL)
				{
					free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
					free(free_blockid);
				}
				
				fb_finish(wpid, finf);
				unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
				return FALSE;
			}

			// new_dirnode is already referenced
			nfree(minf->dinf, curr_node);
			curr_node = nref(minf->dinf, new_dirnode->nodeid);
			dir_node->n.next_node = *newdir_nodeid;
		
			finf->current_node = curr_node;
			finf->current_block = 0;
			curr_block = 0;
			curr_block_cursor = 0;
			dir_node->n.blocks[curr_block] = *free_blockid;
		}
		second_block = 1;
	}
	fb_finish(wpid, finf);
	
	free(finf);
	nfree(minf->dinf, curr_node);

	// preserve new dir node if any
	if(new_dirnode != NULL && !write_node(new_dirnode, minf, wpid, command, ret))
	{
		if(free_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
		}
		free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
		free(newdir_nodeid);

		if(new_dirnode != NULL)
		{
			nfree(minf->dinf, new_dirnode);
			dir_node->n.next_node = OFS_NULL_VALUE;
		}
		nfree(minf->dinf, dir_node);
		
		if(*out_newnode != NULL) free(*out_newnode);
		*out_newnode = NULL;
		if(free_blockid != NULL)
		{
			free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
			free(free_blockid);
		}
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return FALSE;
	}
	else if(directory_node->nodeid != dir_node->nodeid && !write_node(dir_node, minf, wpid, command, ret))
	{
		if(free_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
		}
		if(newdir_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
			free(newdir_nodeid);
		}
		if(new_dirnode != NULL)
		{
			nfree(minf->dinf, new_dirnode);
			dir_node->n.next_node = OFS_NULL_VALUE;
		}
		nfree(minf->dinf, dir_node);
		
		if(*out_newnode != NULL) free(*out_newnode);
		*out_newnode = NULL;
		if(free_blockid != NULL)
		{
			free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
			free(free_blockid);
		}
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return FALSE;
	}

	unsigned long long oldfilesize = directory_node->n.file_size;
	directory_node->n.file_size = directory_node->n.file_size + dentry->record_length;

	// prserve base node
	if(!write_node(directory_node, minf, wpid, command, ret))
	{
		directory_node->n.file_size = oldfilesize;
		if(free_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *free_nodeid, minf, command, wpid, ret);
			free(free_nodeid);
		}
		if(newdir_nodeid != NULL)
		{
			free_node(TRUE, force_preservation, *newdir_nodeid, minf, command, wpid, ret);
			free(newdir_nodeid);
		}
		if(new_dirnode != NULL)
		{
			nfree(minf->dinf, new_dirnode);
			dir_node->n.next_node = OFS_NULL_VALUE;
		}
		nfree(minf->dinf, dir_node);
		
		if(*out_newnode != NULL) free(*out_newnode);
		*out_newnode = NULL;
		if(free_blockid != NULL)
		{
			free_block(TRUE, force_preservation, *free_blockid, minf, command, wpid, ret);
			free(free_blockid);
		}
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return FALSE;
	}

	// insert new node references on tree
	wait_mutex(&minf->dinf->nodes_mutex);
	avl_insert(&minf->dinf->nodes, *out_newnode, (*out_newnode)->nodeid);
	leave_mutex(&minf->dinf->nodes_mutex);
	
	if(!linking)
	{
		// lock new node
		// NOTE: This lock cannot fail for the node cannot be accesed 
		// because the dir is still with an exclusive lock
		lock_node(*free_nodeid, OFS_NODELOCK_EXCLUSIVE, OFS_LOCKSTATUS_DENYALL, wpid, command, ret);
	}

	// unlock dir node
	unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);

	if(free_nodeid != NULL) free(free_nodeid);
	if(newdir_nodeid != NULL) free(newdir_nodeid);
		
	if(new_dirnode != NULL) nfree(minf->dinf, new_dirnode);
	nfree(minf->dinf, dir_node);
		
	if(free_blockid != NULL) free(free_blockid);

	// cache path
	cache_path_node(&minf->dinf->pcache, file_name, (*out_newnode)->nodeid, flags);

	return TRUE;
}
