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

/* Close message implementation */
struct stdfss_res *close_file(int wpid, struct working_thread *thread, struct stdfss_close *close_cmd)
{
	struct stdfss_res *ret = NULL;
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct file_buffer *fbuffer = NULL;
	AvlTree *sub_avl = NULL;
	CPOSITION it, cit;
	struct stask_file_info *tfinf = NULL;

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, close_cmd->file_id, &finf, &tinf))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	// if it was open on write or append mode, preserve buffers
	// and nodes if modified
	if(finf->dinf->dev_type != DEVTYPE_CHAR && finf->buffered && finf->file_modified && ((finf->mode & STDFSS_FILEMODE_APPEND) || (finf->mode & STDFSS_FILEMODE_WRITE)))
	{
		// preserve buffer
		if(finf->assigned_buffer != NULL && fb_lock(finf->assigned_buffer, wpid))
		{
			if(!fb_write(wpid, finf, &ret))
			{
				fb_finish(wpid, finf);
				return ret;
			}
				
			// free current file buffer
			fb_free(wpid, finf);
		}

		// preserve nodes
		// if its not a device opened as a file
		if(finf->dinf->mount_info != NULL)
		{
			if(finf->current_node != NULL) write_node(finf->current_node, finf->dinf->mount_info, wpid, thread->command.command, &ret);

			if(finf->current_node->nodeid != finf->file_base_node->nodeid)
			{
				write_node(finf->file_base_node, finf->dinf->mount_info, wpid, thread->command.command, &ret);
			}
		
			// preserve bitmap changes
			preserve_blocksbitmap_changes(TRUE, finf->dinf->mount_info, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
			}
			preserve_nodesbitmap_changes(TRUE, finf->dinf->mount_info, thread->command.command, wpid, &ret);
			if(ret != NULL)
			{
				return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
			}
		}
	}
	else if(finf->assigned_buffer != NULL && fb_lock(finf->assigned_buffer, wpid))
	{	
		// free current file buffer
		fb_free(wpid, finf);
	}

	// release the device 
	wait_mutex(&opened_files_mutex);
	{
		if(finf->dinf->mount_info == NULL)
		{
			finf->dinf->open_count--;

			if(lock_device(wpid, finf->deviceid, finf->logic_deviceid))
			{
				free_device(wpid, finf->deviceid, finf->logic_deviceid, finf->dinf, thread->command.command, &ret);
				unlock_device(wpid);
			}
		}

		avl_remove(&tinf->open_files, finf->fileid);
		
		if(finf->current_node != NULL) nfree(finf->dinf, finf->current_node);
		if(finf->file_base_node != NULL) nfree(finf->dinf, finf->file_base_node);
		
		// remove from opened files
		it = get_head_position(&opened_files);
		
		while(it != NULL)
		{
			cit = it;
			tfinf = (struct stask_file_info *)get_next(&it);
			
			if(tfinf->fileid == finf->fileid && finf->taskid == tfinf->taskid)
			{
				remove_at(&opened_files, cit);
				break;
			}
		}

		free(finf);
		if(avl_get_total(&tinf->open_files) == 0)
		{
			avl_remove(&tasks, thread->taskid);
			free(tinf);
		}
	}
	leave_mutex(&opened_files_mutex);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}		
