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

/* Filebuffers Ver 2.0 */

/* Jun 29 2005: Added support for cached file buffers. */

int fb_read(int wpid, struct stask_file_info *finf, struct stdfss_res **ret);
void fb_lock_internal(struct file_buffer *fbuffer, int wpid);
void fb_unlock_internal(struct file_buffer *fbuffer, int on_mutex, int wpid);
void fb_unlock(struct file_buffer *fbuffer, int wpid);
void fb_updatelba(struct sdevice_info *dinf, struct file_buffer *fbuffer, unsigned int newlba);
int fb_credits(struct file_buffer *buffer);
CPOSITION fb_getfpos(struct stask_file_info *finf);
void fb_clear(struct file_buffer *fbuffer);

/*
	This function gets a file buffer for the file info specified block.
	The block will be calculated based on finf fields.
	If the device has a mount_info structure the lba will be taken from the current_node, current_block 
	pair. Else current_block will be used.
	If fill is TRUE, file buffer contents will be read from the device. Else the buffer is set to
	0s.
*/
int fb_get(int wpid, int command, struct stask_file_info *finf, int fill, struct stdfss_res **ret)
{
	struct stask_file_info *ownerfinf = NULL;
	struct file_buffer *buff = NULL;
	struct sdevice_info *bdinf = NULL;
	unsigned int lba, candidate_credits, credits, candidate;
	CPOSITION it;
	int i, *ptr;

	// calculate buffer lba
	if(finf->dinf->mount_info != NULL)
		lba = finf->current_node->n.blocks[finf->current_block];
	else
		lba = finf->current_block;

	wait_mutex(&file_buffers_mutex);

	// if it has a buffer and lba is ok lock the buffer
	if(finf->assigned_buffer != NULL && finf->assigned_buffer->block_lba == lba)
	{
		// NOTE: fb_lock will release the mutex.
		fb_lock_internal(finf->assigned_buffer, wpid);

		// if fill failed..
		if((finf->assigned_buffer->fill_failed || finf->assigned_buffer->invalidated) && !fb_read(wpid, finf, ret))
		{
			finf->assigned_buffer->fill_failed = TRUE;
			fb_unlock_internal(finf->assigned_buffer, FALSE, wpid);
			return FALSE;
		}

		finf->assigned_buffer->fill_failed = FALSE;
		return TRUE;
	}

	// ok, finf does not have a buffer assigned, or lba
	// is not the same, let's see if there is a buffer
	// with requested lba on the device
	buff = (struct file_buffer*)avl_getvalue(finf->dinf->buffers, lba);

	if(buff != NULL)
	{
		// see if finf already has a buffer
		if(finf->assigned_buffer != NULL)
		{
			// if it has a buffer, see if it must be 
			// freed 
			if(length(&finf->assigned_buffer->opened_files) == 1)
			{
				avl_remove(&finf->dinf->buffers, finf->assigned_buffer->block_lba);
				// reset the buffer (there won't be WP waiting for lock for
				// it's the only owner)
				fb_reset(finf->assigned_buffer);
			}
			else
			{
				// remove this finf from buffer references.
				// unlock the buffer.
				it = fb_getfpos(finf);
				if(it != NULL) remove_at(&finf->assigned_buffer->opened_files, it);
				// unlock the buffer
				// NOTE: if there were threads waiting for lock
				// this buffer won't be a candidate for removal, because
				// lock is granted here.
				// if there were not threads waiting for lock, buffer
				// might be removed from them, but it'll be ok.
				fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
			}
			finf->assigned_buffer = NULL;
		}	

		// assign buff
		add_tail(&buff->opened_files, finf);
		
		finf->assigned_buffer = buff;

		fb_lock_internal(finf->assigned_buffer, wpid);

		// I won't fill the buffer for it has already been filled 
		// or written to by other WP before
		// if fill failed..
		if((finf->assigned_buffer->fill_failed || finf->assigned_buffer->invalidated) && !fb_read(wpid, finf, ret))
		{
			finf->assigned_buffer->fill_failed = TRUE;
			fb_unlock_internal(finf->assigned_buffer, FALSE, wpid);
			return FALSE;
		}

		finf->assigned_buffer->fill_failed = FALSE;

		return TRUE;
	}
	else if(finf->assigned_buffer != NULL && length(&finf->assigned_buffer->opened_files) == 1)
	{
#ifdef OFS_FILEBUFFER_REUSE
		// with reuse, we have to check if block is cached and unused
		avl_remove(&finf->dinf->buffers, finf->assigned_buffer->block_lba);
		// reset the buffer (there won't be WP waiting for lock for
		// it's the only owner)
		fb_reset(finf->assigned_buffer);

		finf->assigned_buffer = NULL;

#else
		// if it has a buffer but lba is not the same then
		// if its the only owner of the buffer, change buffer lba, update 
		// buffers avl and fill it if necesary.
		fb_updatelba(finf->dinf, finf->assigned_buffer, lba);

		// lock the buffer
		fb_lock_internal(finf->assigned_buffer, wpid);

		if((finf->assigned_buffer->fill_failed || finf->assigned_buffer->invalidated || fill) && !fb_read(wpid, finf, ret))
		{
			finf->assigned_buffer->fill_failed = TRUE;
			fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
			finf->assigned_buffer = NULL;
			fb_free(wpid, finf); // free the buffer
			return FALSE;
		}

		finf->assigned_buffer->fill_failed = FALSE;

		return TRUE;
#endif
	}
	else if(finf->assigned_buffer != NULL)
	{
		// remove this finf from buffer references.
		it = fb_getfpos(finf);
		if(it != NULL) remove_at(&finf->assigned_buffer->opened_files, it);
		// unlock the buffer
		fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
		
		finf->assigned_buffer = NULL;

	}

	i = 0;

	// It has no buffer and there is no available 
	// buffer with requested lba taken by another finf
	int candidate_buffer = -1;
	int cached = 0;
	unsigned int hits = (unsigned int)-1;
#ifdef OFS_FILEBUFFER_REUSE
	while(i < OFS_FILEBUFFERS)
	{
		// see if there is a free buffer with same lba (cached)
		if(length(&file_buffers[i].opened_files) == 0)
		{
			if(finf->logic_deviceid == file_buffers[i].logic_deviceid && finf->deviceid == file_buffers[i].deviceid && lba == file_buffers[i].block_lba)
			{
				candidate_buffer = i;
				cached = 1;

				break;			
			}
			else if(file_buffers[i].block_lba == (unsigned int)-1)
			{
				if(file_buffers[i].hits < hits)
				{

					candidate_buffer = i;
					hits = file_buffers[i].hits;
				}
			}
		}
		i++;
	}

	if(candidate_buffer == -1)
	{
	i = 0;
	hits = (unsigned int)-1;
#endif
	while(i < OFS_FILEBUFFERS)
	{
		// see if there is a free buffer
		if(length(&file_buffers[i].opened_files) == 0 && file_buffers[i].block_lba == (unsigned int)-1)
		{
			candidate_buffer = i;
			break;
		}
#ifdef OFS_FILEBUFFER_REUSE
		else if(length(&file_buffers[i].opened_files) == 0 && file_buffers[i].block_lba != (unsigned int)-1)
		{
			if(file_buffers[i].hits < hits)
			{

				candidate_buffer = i;
				hits = file_buffers[i].hits;
			}
		}
#endif
		i++;
	}
#ifdef OFS_FILEBUFFER_REUSE
	}
#endif	

	if(candidate_buffer != -1)
	{
		fb_reset(&file_buffers[candidate_buffer]);

		if(!cached)
		{
			file_buffers[candidate_buffer].invalidated = FALSE;
			fb_clear(&file_buffers[candidate_buffer]);
		}
		add_tail(&file_buffers[candidate_buffer].opened_files, finf);
		file_buffers[candidate_buffer].hits = 1;
		file_buffers[candidate_buffer].deviceid = finf->deviceid;
		file_buffers[candidate_buffer].logic_deviceid = finf->logic_deviceid;
		file_buffers[candidate_buffer].block_lba = lba;
		file_buffers[candidate_buffer].lockingthreadid = wpid; // lock it			
		file_buffers[candidate_buffer].waiting_count = 0;
		file_buffers[candidate_buffer].waiting_start = 0;

		finf->assigned_buffer = &file_buffers[candidate_buffer];

		avl_insert(&finf->dinf->buffers, finf->assigned_buffer, lba);

		fb_lock_internal(finf->assigned_buffer, wpid);

		if((finf->assigned_buffer->fill_failed || finf->assigned_buffer->invalidated || (fill & !cached)) && !fb_read(wpid, finf, ret))
		{
			finf->assigned_buffer->fill_failed = TRUE;
			fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
			finf->assigned_buffer = NULL;
			fb_free(wpid, finf); // free the buffer
			return FALSE;
		}
		finf->assigned_buffer->fill_failed = FALSE;

		return TRUE;
	}

	// No free buffers left, find a candidate for removal.
	// Removal candidates will be those buffers which 
	//are not locked and credits are the lowest.
	i = 0;
	candidate_credits = (unsigned int)-1;
	while(i < OFS_FILEBUFFERS)
	{
		credits = fb_credits(&file_buffers[i]);
		if(candidate_credits > credits && file_buffers[i].lockingthreadid == -1)
		{
			candidate = i;
		}
		i++;
	}

	// ok got our candidate, lock it
	finf->assigned_buffer = &file_buffers[candidate];

	finf->assigned_buffer->lockingthreadid = wpid;
	finf->assigned_buffer->waiting_count = 0;
	finf->assigned_buffer->waiting_start = 0;

	// release the buffer from it's previous owners
	it = get_head_position(&finf->assigned_buffer->opened_files);

	while(it != NULL)
	{
		ownerfinf = (struct stask_file_info *)get_next(&it);

		ownerfinf->assigned_buffer = NULL;
	}

	// if buffer device is different from current
	// remove the buffer from the buffers avl tree.
	if(finf->deviceid != finf->assigned_buffer->deviceid || finf->logic_deviceid != finf->assigned_buffer->logic_deviceid)
	{
		bdinf = ownerfinf->dinf;
		avl_remove(&bdinf->buffers, finf->assigned_buffer->block_lba);
	}

	lstclear(&finf->assigned_buffer->opened_files);

	if(!fb_write(wpid, finf, ret))
	{
		// an error ocurred while comunicating with device
		// leave the buffer as it was.. perhaps other thread
		// might have some luck
		// Let the buffer in a free state. As we released the mutex 
		finf->assigned_buffer->lockingthreadid = -1;
		finf->assigned_buffer = NULL;
		leave_mutex(&file_buffers_mutex);
		return FALSE;
	}

	// The buffer we got is locked... that means it won't be taken
	// by other working thread, and thats why it's safe to exit the mutex

	// update buffer lba, and insert on device buffers avl
	// NOTE: I think it's important to do this before 
	// releasing the mutex, because other WP could ask for the same
	// lba on the same device, and it wouldn't be on the avl, hence
	// it would be duplicated.
	if(bdinf != NULL)
	{
		finf->assigned_buffer->block_lba = lba;
		avl_insert(&finf->dinf->buffers, finf->assigned_buffer, lba);
	}
	else
	{
		// change buffer lba
		fb_updatelba(finf->dinf, finf->assigned_buffer, lba);
	}

	finf->assigned_buffer->dirty = FALSE;
	finf->assigned_buffer->grace_period = OFS_FILEBUFFER_GRACEPERIOD;
	finf->assigned_buffer->hits = 1;
	finf->assigned_buffer->deviceid = finf->deviceid;
	finf->assigned_buffer->logic_deviceid = finf->logic_deviceid;
	finf->assigned_buffer->invalidated = FALSE;

	add_tail(&finf->assigned_buffer->opened_files, finf);

	leave_mutex(&file_buffers_mutex);

	i = 0;
	ptr = (int*)file_buffers[candidate].buffer;

	// clear info for a safe FS ^^
	while(i < (OFS_FILE_BUFFERSIZE / 4))
	{
		ptr[i] = 0;
		i++;
	}
	
	if(fill && !fb_read(wpid, finf, ret))
	{
		finf->assigned_buffer->fill_failed = TRUE;
		fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
		finf->assigned_buffer = NULL;
		fb_free(wpid, finf); // free the buffer
		return FALSE;
	}

	finf->assigned_buffer->fill_failed = FALSE;

	return TRUE;
}

/*
	This function will unlock the current file buffer. Making it available for
	removal again.
*/
void fb_finish(int wpid, struct stask_file_info *finf)
{
	fb_unlock(finf->assigned_buffer, wpid);
}

/* 
	This function will attempt to lock the current finf filebuffer.
	if filebuffer is NULL it'll return FALSE.
	If buffer is succesfully locked it'll return TRUE.
*/
int fb_lock(struct file_buffer *fbuffer, int wpid)
{
	wait_mutex(&file_buffers_mutex);
	
	// buffer might have been taken away from us
	if(fbuffer == NULL)
	{
		leave_mutex(&file_buffers_mutex);
		return FALSE;
	}

	fb_lock_internal(fbuffer, wpid);

	return TRUE;
}
/*
	This function will free the file info buffer structure. If the buffer still has references
	those references won't be changed.
*/
int fb_free(int wpid, struct stask_file_info *finf)
{
	CPOSITION it;

	wait_mutex(&file_buffers_mutex);

	if(finf->assigned_buffer == NULL)
	{
		leave_mutex(&file_buffers_mutex);	
		return TRUE;
	}

	// buffer must be locked
	if(finf->assigned_buffer->lockingthreadid != wpid)
	{
		leave_mutex(&file_buffers_mutex);
		return FALSE;
	}

	it = fb_getfpos(finf);
	if(it != NULL) remove_at(&finf->assigned_buffer->opened_files, it);

	if(length(&finf->assigned_buffer->opened_files) > 0)
	{
		fb_unlock_internal(finf->assigned_buffer, TRUE, wpid);
		leave_mutex(&file_buffers_mutex);
		finf->assigned_buffer = NULL; // no longer has a buffer
		return TRUE;
	}

	avl_remove(&finf->dinf->buffers, finf->assigned_buffer->block_lba);
	fb_reset(finf->assigned_buffer);

	finf->assigned_buffer = NULL; // no longer has a buffer

	leave_mutex(&file_buffers_mutex);

	return TRUE;
}

/*
	This will preserve buffer contents on the device.
*/
int fb_write(int wpid, struct stask_file_info *finf, struct stdfss_res **ret)
{
	struct stdblockdev_writem_cmd writemcmd;
	struct stdblockdev_res deviceres;
	
	// preserve is performed using the writing cache
	// on file info

	// buffer must be locked in order to allow preservation
	if(finf->assigned_buffer->lockingthreadid != wpid)
	{
		return FALSE;
	}

	// Depending on wheter this is a char or block device
	// send write msg to device
	// write buffer to disk

	if(!finf->assigned_buffer->dirty) return TRUE;

	working_threads[wpid].buffer_wait = 1;

	if(finf->dinf->mount_info == NULL)
	{
		// it's a device opened as a file
		if(finf->dinf->dev_type == DEVTYPE_BLOCK)
		{
			writemcmd.buffer_smo = share_mem(finf->deviceid, (finf->assigned_buffer->buffer + finf->block_offset), finf->dinf->block_size , READ_PERM);
			writemcmd.command = BLOCK_STDDEV_WRITE;
			writemcmd.count = 1;
			writemcmd.dev = finf->logic_deviceid;
			writemcmd.msg_id = wpid;
			writemcmd.pos = finf->assigned_buffer->block_lba;
			writemcmd.ret_port = OFS_BLOCKDEV_PORT;

			locking_send_msg(finf->deviceid, finf->dinf->protocol_port, &writemcmd, wpid);

			claim_mem(writemcmd.buffer_smo);

			get_signal_msg((int*)&deviceres, wpid);

			if(deviceres.ret != STDBLOCKDEV_OK)
			{
				*ret = build_response_msg(working_threads[wpid].command.command, STDFSSERR_DEVICE_ERROR);
				working_threads[wpid].buffer_wait = 0;
				return FALSE;
			}

		}
	}
	else
	{
		// it's a mounted device
		if(!bc_write((char*)finf->assigned_buffer->buffer, OFS_FILE_BUFFERSIZE, finf->assigned_buffer->block_lba, working_threads[wpid].command.command, wpid, finf->dinf->mount_info, FALSE, ret ))
		{	
			working_threads[wpid].buffer_wait = 0;
			return FALSE;
		}

		// if buffer was invalidated, validate it
		wait_mutex(&finf->assigned_buffer->invalidated_mutex);
		finf->assigned_buffer->invalidated = FALSE;
		leave_mutex(&finf->assigned_buffer->invalidated_mutex);
	}

	// free buffer slot
	working_threads[wpid].buffer_wait = 0;

	// set the buffer as not dirty
	finf->assigned_buffer->dirty = FALSE;

	return TRUE;
}

void fb_reset(struct file_buffer *fbuffer)
{
	fbuffer->lockingthreadid = -1;
	fbuffer->dirty = FALSE;
	fbuffer->grace_period = OFS_FILEBUFFER_GRACEPERIOD;
	//fbuffer->hits = 0; // I'll keep hits for caching too
	//fbuffer->block_lba = -1; // we keep the lba, for caching
	//fbuffer->fill_failed = FALSE; // this is kept too
	lstclear(&fbuffer->opened_files);
	fbuffer->waiting_count = 0;
	fbuffer->waiting_start = 0;
}

void fb_clear(struct file_buffer *fbuffer)
{
	int i = 0, size = OFS_FILE_BUFFERSIZE / 4;
	unsigned int *ptr = (unsigned int*)fbuffer->buffer;

	// clear info for a safe FS ^^
	while(i < size)
	{
		ptr[i] = 0;
		i++;
	}
}

void init_file_buffers()
{
	int i = 0;

	while(i < OFS_FILEBUFFERS)
	{
		init(&file_buffers[i].opened_files);
		fb_reset(&file_buffers[i]);
		i++;
		file_buffers[i].block_lba = (unsigned int)-1;
		file_buffers[i].fill_failed = FALSE;
		file_buffers[i].invalidated = FALSE;
		init_mutex(&file_buffers[i].invalidated_mutex);
	}
}

/*
	This function will fill the buffer with the specified block. Now it's ment only for internal use, and WPs 
	shouldn't call this function directly, but through fb_get.
*/
int fb_read(int wpid, struct stask_file_info *finf, struct stdfss_res **ret)
{
	struct stdblockdev_readm_cmd readmcmd;
	struct stdblockdev_res deviceres;
	
	// buffer must be locked in order to fill it
	if(finf->assigned_buffer->lockingthreadid != wpid)
	{
		return FALSE;
	}

	if(finf->dinf->mount_info == NULL)
	{
		// it's a device opened as a file
		if(finf->dinf->dev_type == DEVTYPE_BLOCK)
		{
			readmcmd.buffer_smo = share_mem(finf->deviceid, finf->assigned_buffer->buffer, finf->dinf->block_size, WRITE_PERM);
			readmcmd.command = BLOCK_STDDEV_READ;
			readmcmd.count = 1;
			readmcmd.dev = finf->logic_deviceid;
			readmcmd.msg_id = wpid;
			readmcmd.pos = finf->assigned_buffer->block_lba;
			readmcmd.ret_port = OFS_BLOCKDEV_PORT;

			locking_send_msg(finf->deviceid, finf->dinf->protocol_port, &readmcmd, wpid);

			claim_mem(readmcmd.buffer_smo);

			get_signal_msg((int*)&deviceres, wpid);

			if(deviceres.ret != STDBLOCKDEV_OK)
			{
				*ret = build_response_msg(working_threads[wpid].command.command, STDFSSERR_DEVICE_ERROR);
				return FALSE;
			}

		}
	}
	else
	{
		// it's a mounted device
		if(!bc_read((char*)finf->assigned_buffer->buffer, OFS_FILE_BUFFERSIZE, finf->assigned_buffer->block_lba, working_threads[wpid].command.command, wpid, finf->dinf->mount_info, FALSE, ret ))
		{					
			return FALSE;
		}

	}

	// if buffer was invalidated, validate it
	wait_mutex(&finf->assigned_buffer->invalidated_mutex);
	finf->assigned_buffer->invalidated = FALSE;
	leave_mutex(&finf->assigned_buffer->invalidated_mutex);

	return TRUE;
}

/*
	This function is for internal use and will assume file_buffers_mutex has ben locked.
*/
void fb_lock_internal(struct file_buffer *fbuffer, int wpid) 
{
	int index;

	if(fbuffer->lockingthreadid != wpid && fbuffer->lockingthreadid != -1)
	{
		// enter locking waiting list
		index = (fbuffer->waiting_start + fbuffer->waiting_count) % OFS_MAXWORKINGTHREADS;
		fbuffer->processes_waiting_lock[index] = wpid;
		fbuffer->waiting_count++;

		leave_mutex(&file_buffers_mutex);

		while(fbuffer->lockingthreadid != wpid){ reschedule(); }

		return;
	}
	fbuffer->lockingthreadid = wpid;
	if(fbuffer->hits + 1 != (unsigned int)-1) fbuffer->hits++;
	leave_mutex(&file_buffers_mutex);
	return;
}

void fb_unlock(struct file_buffer *fbuffer, int wpid)
{
	if(fbuffer == NULL) return;

	fb_unlock_internal(fbuffer, FALSE, wpid);
}

void fb_unlock_internal(struct file_buffer *fbuffer, int on_mutex, int wpid)
{
	int next;

	if(!on_mutex) wait_mutex(&file_buffers_mutex);

	// check locking wpid is the one unlocking
	if(wpid != fbuffer->lockingthreadid)
	{
		if(!on_mutex) leave_mutex(&file_buffers_mutex);
		return;
	}

	// see if there was someone waiting
	if(fbuffer->waiting_count == 0) 
	{
		fbuffer->lockingthreadid = -1;
		if(!on_mutex) leave_mutex(&file_buffers_mutex);
		return;
	}

	// get next from waiting list
	next = fbuffer->processes_waiting_lock[fbuffer->waiting_start];

	fbuffer->waiting_start = (fbuffer->waiting_start + 1) % OFS_MAXWORKINGTHREADS;
	fbuffer->waiting_count--;
	fbuffer->lockingthreadid = next;
	fbuffer->hits++;

	if(!on_mutex) leave_mutex(&file_buffers_mutex);
}

void fb_updatelba(struct sdevice_info *dinf, struct file_buffer *fbuffer, unsigned int newlba)
{
	avl_remove(&dinf->buffers, fbuffer->block_lba);
	fbuffer->block_lba = newlba;
	avl_insert(&dinf->buffers, fbuffer, newlba);
}
CPOSITION fb_getfpos(struct stask_file_info *finf)
{
	struct stask_file_info *lfinf;
	CPOSITION it, oldit;

	it = get_head_position(&finf->assigned_buffer->opened_files);

	while(it != NULL)
	{
		oldit = it;
		lfinf = (struct stask_file_info *)get_next(&it);

		if(lfinf->taskid == finf->taskid && lfinf->fileid == finf->fileid)
		{
			return oldit;
		}
	}
	return NULL;
}
int fb_credits(struct file_buffer *buffer)
{
	if(buffer->hits > OFS_MAXFILEBUFFER_HITS)
	{
		return 0;
	}
	if(buffer->grace_period > 0) buffer->grace_period--;
	return buffer->hits * buffer->grace_period * length(&buffer->opened_files);
}


