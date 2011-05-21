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

struct stdfss_res *umount_device(int wpid, struct working_thread *thread, struct stdfss_umount *umount_cmd)
{
	CPOSITION it = NULL;
	struct stask_file_info *finf = NULL;
	struct smount_info *minf = NULL;
	struct swaiting_command *waiting_cmd = NULL;
	char *str = NULL, *old_str = NULL;
	struct stdfss_res failure;
	struct stdblockdev_read_cmd block_read;
	struct stdblockdev_write_cmd block_write;
	struct stdblockdev_res block_res;

	// this command will run alone, meaning
	// there are no concurrent comands executing
	// and all commands pending will be canceled if umount
	// is succesfull
	str = get_string(umount_cmd->mount_path);

	struct stdfss_res *ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	// remove trailing / 
	if(last_index_of(str,'/') == len(str) - 1)
	{
		old_str = str;
		str = substring(str, 0, len(str) - 1);
		free(old_str);
	}

    // NOTE: minf will never be null
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue(mounted, str);
    // remove minf from mount tree
	lpt_remove(&mounted, str);
	leave_mutex(&mounted_mutex);
    
	free(str);

	// check there are no opened files on the mounted device
	// if there are opened files umount will fail
	wait_mutex(&opened_files_mutex);
	{
		it = get_head_position(&opened_files);

		while(it != NULL)
		{
			finf = (struct stask_file_info *)get_next(&it);
			
			if(finf->dinf->mount_info == minf)
			{
				wait_mutex(&minf->dinf->umount_mutex);
				minf->dinf->umount = FALSE;
				leave_mutex(&minf->dinf->umount_mutex);

				return build_response_msg(thread->command.command, STDFSSERR_DEVICE_INUSE);
			}
		}
	}
	leave_mutex(&opened_files_mutex);
	
	// send a failure response to all remaining commands on 
	// the device (those would be open, delete, mkdir, umount, etc 
	// but not file operations for there are no open files
	failure.ret = STDFSSERR_DEVICE_NOT_MOUNTED;

	it = get_head_position(&minf->dinf->waiting);

	while(it != NULL)
	{
		waiting_cmd = (struct swaiting_command *)get_next(&it);
		
		failure.command = waiting_cmd->command.command;

		// send STDERR_DEVICE_NOTMOUNTED
		send_msg(waiting_cmd->sender_id, waiting_cmd->command.ret_port, &failure);
	}

	// remove the device from devices cache so no commands are issued on this device
	remove_cached_deviceE(minf->deviceid, minf->logic_deviceid);

	// decrement mount count
	// clear the dir buffer
	clear_dir_buffer(thread->directory_buffer.buffer);

	// read the OFST from the device //
	block_read.buffer_smo = share_mem(minf->deviceid, thread->directory_buffer.buffer, OFS_BLOCKDEV_BLOCKSIZE, WRITE_PERM);
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = minf->logic_deviceid;
	block_read.msg_id = wpid;
	block_read.pos = minf->dinf->ofst_lba;
	block_read.ret_port = OFS_BLOCKDEV_PORT;

	locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &block_read, wpid);

	get_signal_msg((int *)&block_res, wpid);

	claim_mem(block_read.buffer_smo);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		// ignore the result, if not structures would be messed up
	}

	// copy write_smo data to the ofst //
	mem_copy(thread->directory_buffer.buffer, (unsigned char *)&minf->ofst, sizeof(struct OFST));
		
	// modify mount count - TODO: check of mount count = 0 (?)
	minf->ofst.mount_count--;

	// copy OFST back to the buffer //
	mem_copy((unsigned char *)&minf->ofst, thread->directory_buffer.buffer, sizeof(struct OFST));
	
	// write OFST to device //
	block_write.buffer_smo = share_mem(minf->deviceid, thread->directory_buffer.buffer, 512, READ_PERM);
	block_write.command = BLOCK_STDDEV_WRITE;
	block_write.dev = minf->logic_deviceid;
	block_write.msg_id = wpid;
	block_write.pos = minf->dinf->ofst_lba;
	block_write.ret_port = OFS_BLOCKDEV_PORT;

	locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &block_write, wpid);

	get_signal_msg((int *)&block_res, wpid);

	claim_mem(block_write.buffer_smo);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		// ignore the error
	}

	// remove all waiting commands to prevent starting a command when the umount flag is cleared 
	remove_all(&minf->dinf->waiting);

	wait_mutex(&minf->dinf->umount_mutex);
	minf->dinf->umount = FALSE;
	leave_mutex(&minf->dinf->umount_mutex);

	// free resources used by mount info struct
	// this will also free the device
	avl_remove(&minf->dinf->nodes, 0);
	free_mountinfo_struct(minf, wpid);
	free(minf);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
