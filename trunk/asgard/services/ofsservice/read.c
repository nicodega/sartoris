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
			
struct stdfss_res *read_file(int wpid, struct working_thread *thread, struct stdfss_read *read_cmd, int delimited)
{
	struct stdfss_res *ret = NULL;
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct smount_info *minf = NULL;
	unsigned long long offset = 0, left = 0, old_pos = 0;
	int old_block;
	struct gc_node *old_node = NULL;
	int left_to_read = 0, read, ret_read = 0, eof = 0;
	unsigned int boff, i;
	char *bcheck, nullbuff = '\0', character;

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, read_cmd->file_id, &finf, &tinf))
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
		ret->param1 = 0;
		ret->param2 = 0; 
		return ret;
	}

	// check file was opened for read
	if(!(finf->mode | STDFSS_FILEMODE_READ))
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

	// if reading nothing... then its ok
	if(read_cmd->count == 0 || (delimited && read_cmd->count == 1))
	{
		if(delimited) write_mem(read_cmd->smo, ret_read, 1, &nullbuff); 
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

	/* tinf has task info and finf has file info. */
	// initialize current node if needed
	if(finf->current_node == NULL && finf->dinf->mount_info != NULL)
	{
		finf->current_node = nref(minf->dinf, finf->file_base_node->nodeid);
		finf->current_block = 0;
		finf->cursor_possition = 0;
	}

	if(finf->file_base_node->n.file_size != 0 && finf->current_node->n.blocks[finf->current_block] == OFS_NULL_VALUE)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FS_ERROR);
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

	if(delimited) read_cmd->count--;

	if(finf->dinf->mount_info != NULL)
	{		
		// offset on task buffer
		offset = 0;
		// left holds the ammount of bytes remaining
		left = MIN ( read_cmd->count, finf->file_base_node->n.file_size - finf->cursor_possition );

		old_pos = finf->cursor_possition;
		old_block = finf->current_block;
		old_node = nref(minf->dinf, finf->current_node->nodeid);

		// calculate how much is left on the current block for reading
		// read: the current offset on the file buffer
		// left to read: how many bytes are left on the current block
		// ret_read hold bytes read so far
		read = (int)(finf->cursor_possition % (unsigned long long)OFS_BLOCK_SIZE);
		left_to_read = OFS_BLOCK_SIZE - read;
		ret_read = 0;

		while(left > 0)
		{
			// get the block onto a file buffer
			if(!(finf->dinf->dev_type == DEVTYPE_CHAR) && !fb_get(wpid, thread->command.command, finf, TRUE, &ret))
			{			
				finf->cursor_possition = old_pos;
				finf->current_block = old_block;
				nfree(minf->dinf, finf->current_node);
				finf->current_node = old_node;

				ret->param1 = ret_read;
				ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); 
				return ret;
			}
			bcheck = (char*)(finf->assigned_buffer->buffer + read);

			// if delimited, attempt to cap left_to_read.
			if(delimited)
			{
				// look on the file buffer for \n
				i = 0;
				while(bcheck[i] != '\n' && i < (int)MIN( (unsigned int)OFS_BLOCK_SIZE-read, (unsigned int)left )){i++;}

				if(i != (int)MIN( (unsigned int)OFS_BLOCK_SIZE - read, (unsigned int)left ))
				{
					left = i+1;
				}
			}

			if(read_cmd->command == STDFSS_GETC)
			{
				character = *bcheck;
			}
			else
			{
				// writes on the task buffer as many bytes as possible
				write_mem(read_cmd->smo, (int)offset, 
					(int)MIN( (unsigned int)(OFS_BLOCK_SIZE - read), (unsigned int)left ), 	/* size */
					bcheck		/* buffer */
				);
			}

			// increment cursor possition, offset and bytes read so far by bytes read
			finf->cursor_possition += MIN( OFS_BLOCK_SIZE - read, left );
			offset += MIN( OFS_BLOCK_SIZE - read, left );
			ret_read += MIN( OFS_BLOCK_SIZE - read, (int)left );
			// decrement left on bytes read
			left -= MIN( OFS_BLOCK_SIZE - read, left );
			read = 0; 

			// fix current block (use mod PTRS_ON_NODE + 1 to detect node change)
			finf->current_block = (unsigned int)((finf->cursor_possition / OFS_BLOCK_SIZE) % (PTRS_ON_NODE + 1));

			// if block is greater or equal than 
			// PTRS_ON_NODE, fetch next node
			// and update file info structure
			if(finf->current_block == PTRS_ON_NODE && finf->current_node->n.next_node != NULL)
			{
				// ok next node must be fetched
				if(!read_node(&finf->current_node, finf->current_node->n.next_node, minf, wpid, thread->command.command, &ret))
				{
					fb_finish(wpid, finf);

					finf->cursor_possition = old_pos;
					finf->current_block = old_block;
					nfree(minf->dinf, finf->current_node);
					finf->current_node = old_node;

					ret->param1 = ret_read;
					ret->param2 = finf->file_base_node->n.file_size == finf->cursor_possition;
					return ret;
				}
				
				finf->current_block = 0;
			}
		}

		nfree(minf->dinf, old_node);
	}
	else
	{
		// reading from a device
		if(finf->dinf->dev_type == DEVTYPE_CHAR)
		{
            // its a char device
			
			// ok... this has a big difference with block devices, because 
			// read could block until there's some input. We will send the msg and
			// depending on the command (READ, GETS, GETC). only GETS will be blocking
			return chardev_read(wpid, thread, read_cmd, delimited, finf);
		}
		else
		{
			// it's a block device
			offset = 0; // ofset on the task buffer
			left = read_cmd->count;
			ret_read = 0;
			boff = finf->block_offset;
			
			while(left > 0)
			{
				// get a block from the device
				if(!fb_get(wpid, thread->command.command, finf, TRUE, &ret))
				{
					ret->param1 = ret_read;
					ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
					return ret;
				}

				bcheck = (char*)finf->assigned_buffer->buffer + boff;

				// if delimited, attempt to cap left_to_read.
				if(delimited)
				{
					// look on the file buffer for \n
					i = 0;
					while(bcheck[i] != '\n' && i < (int)MIN( (unsigned int)finf->dinf->block_size - boff, (unsigned int)left)){i++;}
					if(i != (int)MIN( (unsigned int)finf->dinf->block_size - boff, (unsigned int)left))
					{
						left = i+1;
					}
				}

				if(read_cmd->command == STDFSS_GETC)
				{
					character = *bcheck;
				}
				else
				{
					// copy data to the task buffer
					write_mem(read_cmd->smo, (int)offset, MIN( (unsigned int)finf->dinf->block_size - boff, (unsigned int)left), bcheck);
				}

				finf->current_block++;
				left = MIN(finf->dinf->block_size - boff, left);
				ret_read += MIN(finf->dinf->block_size - boff, (unsigned int)left);
				boff = 0;
				offset += ret_read;
			}
			
			finf->block_offset = (read_cmd->count + finf->block_offset) % finf->dinf->block_size;
		}
	}
	fb_finish(wpid, finf);

	ret = build_response_msg(thread->command.command, STDFSSERR_OK);
	ret->param1 = ret_read;
	if(finf->dinf->mount_info == NULL)
	{
		ret->param2 = (finf->dinf->max_lba == finf->current_block && finf->block_offset == finf->dinf->block_size); 
	}
	else
	{
		ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); 
	}

	if(read_cmd->command == STDFSS_GETC)
	{
		((struct stdfss_getc_res*)ret)->c = character;
	}

	// if reading was delimited, append \0 to the end of the buffer
	if(delimited) write_mem(read_cmd->smo, ret_read, 1, &nullbuff); 

	return ret;
}			
