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

/* Shuts down the service. This means freeing allocated structures, canceling 
waiting messages, destroying working threads and closing all mutexes */
void shutdown()
{
	int i = 0;

	// destroy all threads //
	i = 0;

	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].initialized == 1)
		{
			// destroy the thread //
			close_mutex(&working_threads[i].waiting_for_signal_mutex);
			destroy_working_thread(i);
		}
		i++;
	}

	// send an error message to all processes using the OFS Service //
	cancel_waiting_commands();

	// close all system mutexes //
	close_mutex(&idle_threads_mutex);
	close_mutex(&mounted_mutex);
	close_mutex(&node_lock_mutex);
	close_mutex(&opened_files_mutex);
	close_mutex(&file_buffers_mutex);
	close_mutex(&device_lock_mutex);
	close_mutex(&max_directory_msg_mutex);

	close_filebuffer_mutexes();
	close_working_threads_mutexes();
	
	// preserve file buffers //
	preserve_file_changes();

	// free all used structures remaining on memory //
	free_all_structures();

	// close multithreaded malloc mutex
	close_malloc_mutex();

	// enter an infinite loop
	while(1){reschedule();}
}


/* close_mutexes() : closes all global mutexes on the service */
void close_mutexes()
{
	close_mutex(&idle_threads_mutex);
	close_mutex(&mounted_mutex);
	close_mutex(&node_lock_mutex);
	close_mutex(&file_buffers_mutex);
	
	// remember to close all idle devices mutexes as well as thread mutexes
	close_working_threads_mutexes();

	// close multithreaded malloc mutex
	close_malloc_mutex();
}

/* Closes file buffer mutexes, for all filebuffers */
void close_filebuffer_mutexes()
{

}

/* Closes working thread struct mutexes for all working threads */
void close_working_threads_mutexes()
{
	int i = 0;
	
	while(i < OFS_MAXWORKINGTHREADS)
	{
		close_mutex(&working_threads[i].waiting_for_signal_mutex);
		destroy_working_thread(i);
		i++;
	}
	
}

/* Preserves all dirty writing buffers on disk */
void preserve_file_changes()
{
	struct stask_file_info *finf;
	struct stdblockdev_writem_cmd writemcmd;
	struct stdblockdev_res deviceres;
	unsigned int bytes_left = 0, current_byte = 0;
	int id = -1;
	CPOSITION it;

	it = get_head_position(&opened_files);

	while(it != NULL)
	{
		finf = (struct stask_file_info *)get_next(&it);
		if(finf->buffered && finf->assigned_buffer != NULL && finf->assigned_buffer->dirty && ((finf->mode && STDFSS_FILEMODE_APPEND) || (finf->mode && STDFSS_FILEMODE_WRITE)))
		{
			// preserve buffer changes
			if(finf->dinf->mount_info == NULL)
			{
				// it's a device opened as a file
				if(finf->dinf->dev_type == DEVTYPE_BLOCK)
				{
					writemcmd.buffer_smo = share_mem(finf->deviceid, finf->assigned_buffer->buffer, OFS_FILE_BUFFERSIZE, READ_PERM);
					writemcmd.command = BLOCK_STDDEV_WRITEM;
					writemcmd.count = OFS_BLOCKDEVBLOCK_SIZE;
					writemcmd.dev = finf->logic_deviceid;
					writemcmd.msg_id = 0;
					writemcmd.pos = finf->current_block;
					writemcmd.ret_port = OFS_BLOCKDEV_PORT;

					send_msg(finf->deviceid, finf->dinf->protocol_port, &writemcmd);

					id = -1;

					while(id != finf->deviceid)
					{
						while(get_msg_count(OFS_BLOCKDEV_PORT) == 0);

						get_msg(OFS_BLOCKDEV_PORT, &deviceres, &id);
					}
					
					claim_mem(writemcmd.buffer_smo);
					
				}
				else if(finf->dinf->dev_type == DEVTYPE_CHAR)
				{
					// buffering in this version is not supported for char devices
				}
			}
			else
			{
				// it's a mounted device
				writemcmd.buffer_smo = share_mem(finf->deviceid, finf->assigned_buffer->buffer, OFS_FILE_BUFFERSIZE, READ_PERM);
				writemcmd.command = BLOCK_STDDEV_WRITEM;
				writemcmd.count = OFS_BLOCKDEVBLOCK_SIZE;
				writemcmd.dev = finf->logic_deviceid;
				writemcmd.msg_id = 0;
				writemcmd.pos = finf->current_node->n.blocks[finf->current_block];
				writemcmd.ret_port = OFS_BLOCKDEV_PORT;

				send_msg(finf->deviceid, finf->dinf->protocol_port, &writemcmd);

				id = -1;

				while(id != finf->deviceid)
				{
					while(get_msg_count(OFS_BLOCKDEV_PORT) == 0);

					get_msg(OFS_BLOCKDEV_PORT, &deviceres, &id);
				}
				
				claim_mem(writemcmd.buffer_smo);

			}
		}
	}


	// preserve bitmaps
	

	// MISSING CODE HERE
	// TODO: Preserve bitmaps
}

/* Sends an error message to all tasks with waiting commands on a device */
void cancel_waiting_commands()
{
	// for each command on cached devices waiting list send an error message
	int *devices = NULL, *logic_devices = NULL;
	int devices_count, logicdevices_count;
	AvlTree *sub_avl;
	struct sdevice_info *dinf;
	int i = 0, j;
	CPOSITION it;
	struct swaiting_command *waiting_cmd;
	struct stdfss_res *ret;
	
	devices_count = avl_get_indexes(&cached_devices, &devices);

	while(i < devices_count)
	{
		sub_avl = (AvlTree *)avl_getvalue(cached_devices, devices[i]);

		if(sub_avl != NULL)
		{
			logicdevices_count = avl_get_indexes(sub_avl, &logic_devices);

			j = 0;

			while(j < logicdevices_count)
			{
				dinf = (struct sdevice_info *)avl_getvalue(*sub_avl, logic_devices[j]);

				if(length(&dinf->waiting) != 0)
				{
					// send shuting down message
					it = get_head_position(&dinf->waiting);

					while(it != NULL)
					{
						waiting_cmd = (struct swaiting_command*)get_next(&it);

						ret = build_response_msg(waiting_cmd->command.command, STDFSSERR_STDFS_SERVICE_SHUTDOWN);
						ret->thr_id = waiting_cmd->command.thr_id;

						send_msg(waiting_cmd->sender_id, waiting_cmd->command.ret_port, ret);

						free(ret);
					}
				}

				j++;
			}
		}
		i++;
	}

	if(devices != NULL) free(devices);
	if(logic_devices != NULL) free(logic_devices);
}

/* This function will free structures allocated by the service */
void free_all_structures()
{
	// this function frees all structures allocated //
	struct stddev_freedev_cmd freedevice_cmd;
	struct stddev_res response;
	list lst;
	CPOSITION it = NULL;
	struct smount_info *minf = NULL;
	device_info *logic_device = NULL;
	AvlTree *sub_avl = NULL;
	char *key;
	int *devices = NULL, *logic_devices = NULL;
	int devices_count, logicdevices_count;
	int *task_indexes, task_count;
	struct sdevice_info *dinf;
	struct stask_file_info *finf;
	struct stask_info *tinf;
	int i = 0, j = 0, sender;

	freedevice_cmd.command = STDDEV_FREE_DEVICE;
	freedevice_cmd.ret_port = OFS_BLOCKDEV_PORT;
	freedevice_cmd.msg_id = 0;
	

	// free minfs
	lst = lpt_getvalues(mounted);

	it = get_head_position(&lst);

	while(it != NULL)
	{
		minf = (struct smount_info *)get_next(&it);

		sub_avl = (AvlTree *)avl_getvalue(cached_devices, minf->deviceid);
	
		// remove cached device
		remove_cached_deviceE(minf->deviceid, minf->logic_deviceid);

		free_mountinfo_struct(minf, -1);
	}
	
	while(lst.total > 0)
	{
		remove_at(&lst, get_head_position(&lst));
	}

	// remove minfs from tree
	lst = lpt_copy_getkeys(mounted);

	it = get_head_position(&lst);

	while(it != NULL)
	{
		key = (char *)get_next(&it);
		lpt_remove(&mounted, key);
	}

	remove_all(&lst);

	// free all cached devices
	devices_count = avl_get_indexes(&cached_devices, &devices);

	while(i < devices_count)
	{
		sub_avl = (AvlTree *)avl_getvalue(cached_devices, devices[i]);

		if(sub_avl != NULL)
		{
			logicdevices_count = avl_get_indexes(sub_avl, &logic_devices);

			j = 0;

			while(j < logicdevices_count)
			{
				dinf = (struct sdevice_info *)avl_getvalue(*sub_avl, logic_devices[j]);

				// remove device from cached device
				remove_cached_deviceE(devices[i], logic_devices[i]);

				freedevice_cmd.logic_deviceid = logic_devices[i];

				send_msg(devices[i], dinf->stddev_port, &freedevice_cmd);

				while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

				get_msg(OFS_BLOCKDEV_PORT, &response, &sender);

				free_device_struct(dinf);

				j++;
			}
		}
		i++;
	}

	if(devices != NULL) free(devices);
	if(logic_devices != NULL) free(logic_devices);

	// free idle devices
	remove_all(&devs_with_commands);

	// free waiting nodes
	remove_all(&lock_node_waiting);

	// free opened files structs
	it = get_head_position(&opened_files);

	while(it != NULL)
	{
		finf = (struct stask_file_info*)get_next(&it);

		if(finf->current_node != NULL) free(finf->current_node);
	}
	remove_all(&opened_files);

	// free task info
	task_count = avl_get_indexes(&tasks, &task_indexes);
	
	i = 0;

	while(i < task_count)
	{
		tinf = (struct stask_info*)avl_getvalue(tasks, task_indexes[i]);

		avl_remove_all(&tinf->open_files);
		
		i++;
	}

	avl_remove_all(&tasks);
}

