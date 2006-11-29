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

/* This function will flush a given file writing buffer */
struct stdfss_res *flush_buffers(int wpid, struct working_thread *thread, struct stdfss_flush *flush_cmd)
{
	struct stdfss_res *ret = NULL;
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct file_buffer *fbuffer = NULL;
	
	// see if this task has the file opened
	if(!get_file_info(thread->taskid, flush_cmd->file_id, &finf, &tinf))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	// if it was open on write or append mode, preserve its buffer
	// and it's node
	if(finf->buffered && finf->file_modified && ((finf->mode && STDFSS_FILEMODE_APPEND) || (finf->mode && STDFSS_FILEMODE_WRITE)))
	{
		if(fb_lock(finf->assigned_buffer, wpid))
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
		if(finf->dinf->mount_info == NULL)
		{
			// preserves actual node
			if(finf->current_node != NULL) write_node(finf->current_node, finf->dinf->mount_info, wpid, thread->command.command, &ret);

			// preserves base node if different from actual node
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
	
	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
