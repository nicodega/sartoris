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
			
struct stdfss_res *write_file(int wpid, struct working_thread *thread, struct stdfss_write *write_cmd, int delimited)
{
	struct stdfss_res *ret = NULL;
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct smount_info *minf = NULL;
	struct gc_node *n = NULL; 
	unsigned int *new_nodes = NULL, *new_blocks = NULL;
	unsigned long offset = 0, left = 0;
	unsigned long new_node_frombase;
	unsigned long long seek_gap;
	int id, i, j, block_count = 0, node_count = 0, current_block_free = 0, tmp_block;
	int new_node_index = 0, new_block_index = 0;
	int lb_used = 0;
	int written = 0;
	int wasnull = FALSE;

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, write_cmd->file_id, &finf, &tinf))
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
		ret->param1 = 0;
		ret->param2 = 0;
		return ret;
	}

	// check file was opened for write or append 
	if(!((finf->mode & STDFSS_FILEMODE_APPEND) || (finf->mode & STDFSS_FILEMODE_WRITE)))
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
		ret->param1 = 0;
		if(finf->dinf->mount_info == NULL)
		{
			ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
		}
		else
		{
			ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); 
		}
		return ret;
	}

	minf = finf->dinf->mount_info;

	// as this function is not as simple as read, if writing is delimited, we will modify command
	// count now, by reading the user buffer
	if(delimited)
	{
		left = write_cmd->count; 	// remaining
		offset = 0;			// offset on sender buffer
		i = 0;				// count
		
		while(left > 0)
		{
			// read data from buffer, onto dir buffer	
			read_mem(write_cmd->smo, offset, MIN( OFS_BLOCK_SIZE, write_cmd->count), (char*)&thread->directory_buffer.buffer);

			left -= MIN( OFS_BLOCK_SIZE, write_cmd->count);
			
			while(thread->directory_buffer.buffer[i - offset] != '\0' && i < write_cmd->count){i++;}

			if(thread->directory_buffer.buffer[i - offset] == '\0')
				break;

			offset += MIN( OFS_BLOCK_SIZE, write_cmd->count);
		}

		write_cmd->count = i;
	}

	// if writing nothing... then its ok
	if(write_cmd->count == 0)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_OK);
		ret->param1 = 0;
		if(finf->dinf->mount_info == NULL)
		{
			ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
		}
		else
		{
			ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); 
		}
		return ret;
	}

	// initialize file position if needed
	if(finf->dinf->mount_info != NULL)
	{
		if(finf->current_node == NULL || ((finf->mode & STDFSS_FILEMODE_APPEND) && finf->cursor_possition != finf->file_base_node->n.file_size))
		{
			if(finf->mode & STDFSS_FILEMODE_WRITE)
			{
				finf->current_node = nref(minf->dinf, finf->file_base_node->nodeid);
				finf->current_block = 0;
			}
			else
			{
				// appending then set the cursor possition to the end of the file if it has changed
				// get the file's last node id

				// calculate block count
				new_node_frombase = (unsigned int)((unsigned int)(finf->file_base_node->n.file_size / OFS_BLOCK_SIZE)  / PTRS_ON_NODE);
		
				if(finf->current_node != NULL) nfree(minf->dinf, finf->current_node);

				finf->current_node = nref(minf->dinf, finf->file_base_node->nodeid);
				
				while(new_node_frombase > 0)
				{
					if(!read_node(&finf->current_node, finf->current_node->n.next_node, finf->dinf->mount_info, wpid, thread->command.command, &ret))
					{
						if(new_nodes != NULL) free(new_nodes);
						if(new_blocks != NULL) free(new_blocks);
						ret->param1 = 0;
						ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
						return ret;
					}
					
					new_node_frombase--;
				}

				finf->cursor_possition = finf->file_base_node->n.file_size;
				finf->current_block = (unsigned int)((unsigned int)(finf->file_base_node->n.file_size / OFS_BLOCK_SIZE) % PTRS_ON_NODE);
			}
		}	
	}
	
	// check if it's a device
	if(finf->dinf->mount_info != NULL)
	{
		// writing will start at the current cursor possition
		// NOTE: we won't allow cursor possition to be greater than the end of the file
		
		// calculate blocks and nodes necesary for writing data
		if(finf->cursor_possition == finf->file_base_node->n.file_size)
		{
			seek_gap = 0;
			// current possition is the same as file length
			if(finf->current_node->n.blocks[finf->current_block] == OFS_NULL_VALUE)
			{
				current_block_free = OFS_BLOCK_SIZE;
				block_count = 1;
			}
			else
			{
				lb_used = (int)(finf->file_base_node->n.file_size % (unsigned long long)OFS_BLOCK_SIZE);
				current_block_free = OFS_BLOCK_SIZE - lb_used;
				block_count = write_cmd->count - current_block_free;
			}

			if(block_count <= 0)
			{
				block_count = 0;
			}
			else
			{
				block_count = (int)(block_count / OFS_BLOCK_SIZE) + ((block_count % OFS_BLOCK_SIZE == 0)? 0 : 1);
			}

			// calculate new nodes needed
			node_count = 0;
			
			if(block_count != 0)
			{
				i = 0;
				tmp_block = block_count;

				while(i < PTRS_ON_NODE && tmp_block > 0)
				{
					if(finf->current_node->n.blocks[i] == OFS_NULL_VALUE)
					{
						tmp_block--;
					}
					i++;
				}
				node_count = (int)(tmp_block / PTRS_ON_NODE) + ((tmp_block % PTRS_ON_NODE == 0)? 0 : 1);
			}
		}
		else
		{
			// now cursor possition is smaller than file size, so blocks can be re-used
			lb_used = (int)(finf->cursor_possition % (unsigned long long)OFS_BLOCK_SIZE);
			current_block_free = OFS_BLOCK_SIZE - lb_used;
			
			seek_gap = finf->file_base_node->n.file_size - finf->cursor_possition;

			if((write_cmd->count - current_block_free - seek_gap) <= 0)
			{
				block_count = 0; // already have a block wich can be used
			}
			else
			{
				block_count = write_cmd->count - current_block_free - (unsigned int)seek_gap;
				block_count = (int)(block_count / OFS_BLOCK_SIZE) + ((block_count % OFS_BLOCK_SIZE == 0)? 0 : 1);
			}

			// calculate new nodes needed
			node_count = 0;
			
			if(block_count != 0)
			{
				i = 0;
				tmp_block = block_count;

				// get last node of the file
				n = nref(minf->dinf, finf->current_node->nodeid);

				while(n->n.next_node != OFS_NULL_VALUE)
				{
					if(!read_node(&n, n->n.next_node, finf->dinf->mount_info, wpid, thread->command.command, &ret))
					{
						if(new_nodes != NULL) free(new_nodes);
						if(new_blocks != NULL) free(new_blocks);
						ret->param1 = 0;
						ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
						nfree(minf->dinf, n);
						return ret;
					}
				}

				while(i < PTRS_ON_NODE && tmp_block > 0)
				{
					if(n->n.blocks[i] == OFS_NULL_VALUE)
					{
						tmp_block--;
					}
					i++;
				}
				node_count = (int)(tmp_block / PTRS_ON_NODE) + ((tmp_block % PTRS_ON_NODE == 0)? 0 : 1);

				nfree(minf->dinf, n);
			}
		}

		/* block_count holds how many blocks are needed, and node_count, how many new nodes are needed */

		// get free nodes/blocks
		if(node_count != 0)
		{
			new_nodes = get_free_nodes(node_count, !finf->buffered, minf, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				ret->param1 = 0;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}
		}
		else
		{
			new_nodes = NULL;
		}

		if(block_count != 0)
		{
			new_blocks = get_free_blocks(block_count, !finf->buffered, minf, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				if(new_nodes != NULL) free(new_nodes);
				ret->param1 = 0;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}
		}
		else
		{
			new_blocks = NULL;
		}

		
		// In order to get a writing buffer, we have to find out
		// first writing block lba. If the file has just been created 
		// then it's block will be OFS_NULL_VALUE. In that case, we will get a 
		// gree block now, and assign it to the file.
		if(finf->current_node->n.blocks[finf->current_block] == OFS_NULL_VALUE)
		{
			// assign a new block
			finf->current_node->n.blocks[finf->current_block] = new_blocks[new_block_index];
			new_block_index++;
			wasnull = TRUE;
		}

		/* write blocks and nodes */

		// write current block data
		if(current_block_free > 0)
		{
			// get block file buffer
			if(!fb_get(wpid, thread->command.command, finf, (!wasnull && (write_cmd->count < OFS_BLOCK_SIZE)), &ret))
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				ret->param1 = 0;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; 
				return ret;
			}

			wasnull = FALSE;

			// write data from smo onto the buffer
			if(write_cmd->command == STDFSS_PUTC)
			{
				*((char*)(finf->assigned_buffer->buffer + lb_used)) = ((struct stdfss_putc *)write_cmd)->c;
			}
			else
			{
				read_mem(write_cmd->smo, 0, MIN( current_block_free, write_cmd->count), (char*)(finf->assigned_buffer->buffer + lb_used));
			}

			finf->assigned_buffer->dirty = TRUE;
			
			// if not buffered or buffered and there are more blocks to be written
			// preserve the buffer
			if((!finf->buffered || (finf->buffered && write_cmd->count > current_block_free)) && !fb_write(wpid, finf, &ret))
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				fb_finish(wpid, finf);
				ret->param1 = 0;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}
		}
	
		new_block_index = 0;
		new_node_index = 0;				
		
		// write blocks
		
		if(write_cmd->count <= current_block_free)
		{
			left = 0;
		}
		else
		{
			left = write_cmd->count - current_block_free;
		}

		offset = MIN(current_block_free, write_cmd->count);
		written = MIN(current_block_free, write_cmd->count);

		while(left > 0)
		{
			finf->current_block++; // go to next block

			// check if a new node is needed
			if(finf->current_block == PTRS_ON_NODE - 1)
			{
				// ok new node is needed
				// see if nextnode is not null
				if(finf->current_node->n.next_node != NULL)
				{
					id = finf->current_node->n.next_node;
					// use current next node
					if(!read_node(&finf->current_node, finf->current_node->n.next_node, minf, wpid, thread->command.command, &ret))
					{
						if(new_nodes != NULL) free(new_nodes);
						if(new_blocks != NULL) free(new_blocks);
						fb_finish(wpid, finf);
						ret->param1 = written;
						ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
						return ret;
					}

					finf->current_block = 0;
				}
				else
				{
					// set next node on the current node to the one on the array
					finf->current_node->n.next_node = new_nodes[new_node_index];

					// preserve current node
					if(!write_node(finf->current_node, minf, wpid, thread->command.command, &ret))
					{
						if(new_nodes != NULL) free(new_nodes);
						if(new_blocks != NULL) free(new_blocks);
						fb_finish(wpid, finf);
						ret->param1 = written;
						ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
						return ret;
					}

					nfree(finf->dinf, finf->current_node);

					finf->current_node = (struct gc_node*)malloc(sizeof(struct gc_node));
					finf->current_node->ref = 1;
					finf->current_node->nodeid = new_nodes[new_node_index];

					// reset node
					j = 0;
					while(j < PTRS_ON_NODE)
					{
						finf->current_node->n.blocks[j] = OFS_NULL_VALUE;
						j++;
					}

					finf->current_node->n.type = OFS_CHILD;
					finf->current_node->n.creation_date = 0;
					finf->current_node->n.file_size = 0;
					finf->current_node->n.link_count = 0;
					finf->current_node->n.modification_date = 0;
					finf->current_node->n.owner_group_id = finf->file_base_node->n.owner_group_id;
					finf->current_node->n.owner_id = finf->file_base_node->n.owner_id;
					finf->current_node->n.next_node = NULL;

					finf->current_block = 0;
					
					wait_mutex(&finf->dinf->nodes_mutex);
					avl_insert(&finf->dinf->nodes, finf->current_node, finf->current_node->nodeid);
					leave_mutex(&finf->dinf->nodes_mutex);

					new_node_index++;
				}
			}
			
			// if no new block has been allocated, and it's the last block being written
			// or the first block had space remaining
			wasnull = FALSE;

			if(finf->current_node->n.blocks[finf->current_block] == OFS_NULL_VALUE)
			{
				finf->current_node->n.blocks[finf->current_block] = new_blocks[new_block_index];
				new_block_index++;
				wasnull = TRUE;
			}

			if(!fb_get(wpid, thread->command.command, finf, (!wasnull && left < OFS_BLOCK_SIZE), &ret))
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				ret->param1 = 0;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; 
				return ret;
			}

			// read block contents to the buffer
			if(write_cmd->command == STDFSS_PUTC)
			{
				*((char*)finf->assigned_buffer->buffer) = ((struct stdfss_putc *)write_cmd)->c;
			}
			else
			{
				read_mem(write_cmd->smo, offset, MIN( OFS_BLOCK_SIZE, left ), (char*)finf->assigned_buffer->buffer);
			}
			
			finf->assigned_buffer->dirty = TRUE;

			// preserve the buffer
			// NOTE: if buffering is enabled and it's the last block, then the buffer is left dirty
			// and not preserved
			if((finf->buffered && left <= OFS_BLOCK_SIZE) || !finf->buffered)
			{
				if(!fb_write(wpid, finf, &ret))
				{
						if(new_nodes != NULL) free(new_nodes);
						if(new_blocks != NULL) free(new_blocks);
						if(wasnull) finf->current_node->n.blocks[finf->current_block] = OFS_NULL_VALUE; 
						fb_finish(wpid, finf);
						ret->param1 = written;
						ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
						return ret;	
				}
			}

			// update vars
			offset += OFS_FILE_BUFFERSIZE;
			written += MIN( left, OFS_FILE_BUFFERSIZE );
			left -= MIN( left, OFS_FILE_BUFFERSIZE );
		}

		// preserve current node if different from base
		if(node_count != 0)
		{
			// preserve current node
			if(!write_node(finf->current_node, minf, wpid, thread->command.command, &ret))
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				fb_finish(wpid, finf);
				ret->param1 = written;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}

		}

		// update file structure
		if(seek_gap < write_cmd->count)
		{
			finf->file_base_node->n.file_size = finf->file_base_node->n.file_size + (unsigned long long)(write_cmd->count - (unsigned int)seek_gap);
		}
		
		finf->file_modified = TRUE;

		// write basenode to disk
		if(!write_node(finf->file_base_node, minf, wpid, thread->command.command, &ret))
		{
			if(new_nodes != NULL) free(new_nodes);
			if(new_blocks != NULL) free(new_blocks);
			fb_finish(wpid, finf);
			ret->param1 = written;
			ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
			return ret;
		}

		// preserve bitmap changes
		if(block_count != 0)
		{
			preserve_blocksbitmap_changes(finf->buffered, minf, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				fb_finish(wpid, finf);
				ret->param1 = written;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}
		}
		if(node_count != 0)
		{
			preserve_nodesbitmap_changes(finf->buffered, minf, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				if(new_nodes != NULL) free(new_nodes);
				if(new_blocks != NULL) free(new_blocks);
				fb_finish(wpid, finf);
				ret->param1 = written;
				ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition; // check EOF
				return ret;
			}
		}
		
		if(new_nodes != NULL) free(new_nodes);
		if(new_blocks != NULL) free(new_blocks);
		fb_finish(wpid, finf);

		ret = build_response_msg(thread->command.command, STDFSSERR_OK);
		ret->param1 = written;
		ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); // check EOF
		finf->cursor_possition += write_cmd->count;

		return ret;
	}
	else
	{
        // it's a device opened as a file... 
		if(finf->dinf->dev_type == DEVTYPE_CHAR)
		{
			return chardev_write(wpid, thread, write_cmd, delimited, finf);			
		}
		else
		{
			current_block_free = finf->dinf->block_size - finf->block_offset;
			left = write_cmd->count; 
			written = 0;
			offset = 0;

            // write first block
			if(current_block_free > 0)
			{
				// read the contents of the current block (fill will only retrieve one device block)
				if(!fb_get(wpid, thread->command.command, finf, (left < finf->dinf->block_size), &ret))
				{
					ret->param1 = written;
					ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
					return ret;
				}

				// copy data to the buffer
				if(write_cmd->command == STDFSS_PUTC)
				{
					*((char*)finf->assigned_buffer->buffer + finf->block_offset) = ((struct stdfss_putc *)write_cmd)->c;
				}
				else
				{
					read_mem(write_cmd->smo, 0, MIN((unsigned int)current_block_free, left ), (char*)finf->assigned_buffer->buffer + finf->block_offset);
				}

				finf->assigned_buffer->dirty = TRUE;

				// preserve the buffer
				if((!finf->buffered || (finf->buffered && (left + finf->block_offset) > finf->dinf->block_size)) && (!fb_write(wpid, finf, &ret) || ret != NULL) )
				{
					fb_finish(wpid, finf);
				
					ret->param1 = written;
					ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
					return ret;
				}
		
				// update possition
				finf->block_offset += MIN((unsigned int)current_block_free, left );
				if(finf->block_offset == finf->dinf->block_size)
				{
					finf->block_offset = 0;
					finf->current_block++;
				}
				written = MIN(left, (unsigned int)current_block_free); // bytes written to the device
				left -= written;					
				offset = written;  // offset on the task buffer
			}

			while(left > 0)
			{
				// if it's the last block and it's not filled up completely
				// fill the buffer with current contents
				if(!fb_get(wpid, thread->command.command, finf, (left < finf->dinf->block_size), &ret))
				{
					ret->param1 = written;
					ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
					return ret;
				}

				// copy data to the buffer
				if(write_cmd->command == STDFSS_PUTC)
				{
					*((char*)finf->assigned_buffer->buffer) = ((struct stdfss_putc *)write_cmd)->c;
				}
				else
				{
					read_mem(write_cmd->smo, offset, MIN( finf->dinf->block_size, left) , (char*)finf->assigned_buffer->buffer);
				}

				finf->assigned_buffer->dirty = TRUE;

				// preserve the buffer
				if((!finf->buffered || (finf->buffered && left > finf->dinf->block_size) ) && (!fb_write(wpid, finf, &ret) || ret != NULL))
				{
					fb_finish(wpid, finf);
					
					ret->param1 = written;
					ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
		
					return ret;
				}
				
				// update possition
				finf->block_offset += MIN(finf->dinf->block_size, left );
				if(finf->block_offset == finf->dinf->block_size)
				{
					finf->block_offset = 0;
					finf->current_block++;
				}
				written = MIN(left, (unsigned int)finf->dinf->block_size); // bytes written to the device
				left -= written;					
				offset = written;  // offset on the task buffer
			}
		}

		fb_finish(wpid, finf);
		
		ret = build_response_msg(thread->command.command, STDFSSERR_OK);
		ret->param1 = written;
		if(finf->dinf->mount_info == NULL)
		{
			ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
		}
		else
		{
			ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); 
		}
		return ret;
	}
}		
