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
			
struct stdfss_res *seek_file(int wpid, struct working_thread *thread, struct stdfss_seek *seek_cmd)
{
	struct stdfss_res *ret = NULL;
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	unsigned long long new_cursor_position, new_node_frombase, curr_node_frombase;

	// NOTE: this version of the ofs won't allow seeks grater than file size...
	// if such thing is asked for, seek will fail

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, seek_cmd->file_id, &finf, &tinf))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	if(finf->dinf->mount_info == NULL && finf->dinf->dev_type == DEVTYPE_CHAR)
	{
		// send char device seek
		// NOTE: it does not matter whether 
		// the device is blocked or not
		return chardev_seek(wpid, thread, seek_cmd, finf);
	}

	// see if we can seek
	switch(seek_cmd->origin)
	{
	case STDFSS_SEEK_SET:
		new_cursor_position = seek_cmd->possition;
		break;
	case STDFSS_SEEK_CUR:
		new_cursor_position = finf->cursor_possition + seek_cmd->possition;
		break;
	case STDFSS_SEEK_END:

		if(finf->dinf->mount_info == NULL)
		{
			new_cursor_position = (unsigned long long)((unsigned long long)finf->dinf->max_lba * (unsigned long long)finf->dinf->block_size) - seek_cmd->possition;
		}
		else
		{
			if(finf->file_base_node->n.file_size <= seek_cmd->possition)
			{
				ret = build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
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
			new_cursor_position = finf->file_base_node->n.file_size - seek_cmd->possition;
		}
		break;
	default:
		ret = build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND_PARAMS);
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

	// if possition is the same as the cursor return ok
	if(new_cursor_position == finf->cursor_possition)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_OK);
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

	// if position is not valid, fail
	if((finf->dinf->mount_info != NULL && new_cursor_position > finf->file_base_node->n.file_size))
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
		ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); // check EOF

		return ret;
	}

	if(finf->dinf->mount_info != NULL)
	{
		new_node_frombase = (unsigned int)((unsigned int)(new_cursor_position / OFS_BLOCK_SIZE) / PTRS_ON_NODE);
		curr_node_frombase = (unsigned int)((unsigned int)(finf->cursor_possition / OFS_BLOCK_SIZE) / PTRS_ON_NODE);

		if(curr_node_frombase != new_node_frombase)
		{
			nfree(finf->dinf, finf->current_node);
			finf->current_node = nref(finf->dinf, finf->file_base_node->nodeid);

			while(new_node_frombase > 0)
			{
				if(!read_node(&finf->current_node, finf->current_node->n.next_node, finf->dinf->mount_info, wpid, thread->command.command, &ret))
				{
					ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
					ret->param2 = (finf->file_base_node->n.file_size == finf->cursor_possition); // check EOF
					return ret;
				}
				
				new_node_frombase--;
			}
		}
		else if(finf->current_node == NULL)
		{

			finf->current_node = nref(finf->dinf, finf->file_base_node->nodeid);
		}
		finf->current_block = (unsigned int)((new_cursor_position / OFS_BLOCK_SIZE) % PTRS_ON_NODE);
	}
	else
	{
		finf->current_block = (unsigned int)(new_cursor_position / finf->dinf->block_size);
		finf->block_offset = (unsigned int)(new_cursor_position % finf->dinf->block_size);
	}

	finf->cursor_possition = new_cursor_position;
	
	// If file was opened for reading
	// fetch seeked possition buffer 
	// beacuse the task will probably use it soon
	if(finf->buffered && (finf->mode & STDFSS_FILEMODE_READ))
	{
		if(!fb_get(wpid, thread->command.command, finf, TRUE, &ret))
		{
			ret = build_response_msg(thread->command.command, STDFSSERR_OK); // yeap we ignore the fact that we couldn't get a buffer
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
		
		fb_finish(wpid, finf);
	}
	
	ret = build_response_msg(thread->command.command, STDFSSERR_OK);
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
