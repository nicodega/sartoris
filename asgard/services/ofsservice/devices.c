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

int read_device_file(struct gc_node *devicefile_node, struct smount_info *minf, int wpid, int command, int *out_deviceid, int *out_logic_deviceid, struct stdfss_res **ret)
{
	struct sdevice_file *device_file = NULL;

	if(devicefile_node->n.file_size > OFS_BLOCK_SIZE)
	{
		*ret = build_response_msg(command, STDFSSERR_BAD_DEVICE_FILE);
		return FALSE;
	}

	if(!read_buffer((char *)working_threads[wpid].directory_buffer.buffer, OFS_DIR_BUFFERSIZE, devicefile_node->n.blocks[0], command, wpid, minf, ret))
	{
		return FALSE;
	}

	// now the buffer has the device file
	device_file = (struct sdevice_file *)working_threads[wpid].directory_buffer.buffer;

	if(!resolve_name((char*)((unsigned char *)working_threads[wpid].directory_buffer.buffer + sizeof(struct sdevice_file)), out_deviceid, wpid))
	{
		*ret = build_response_msg(command, STDFSSERR_BAD_DEVICE_FILE);
		return FALSE;
	}

	*out_logic_deviceid = device_file->logic_deviceid;

	return TRUE;
}

// this function will request the device and
// fetch device protocol port, etc
int request_device(int wpid, int deviceid, int logic_deviceid, struct sdevice_info *dinf, int request_mode, int command, struct stdfss_res **ret)
{
	struct stddev_cmd stddevcmd;
	struct stddev_devtype_res devtype_res;
	struct stddev_res stddevres;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	struct stddev_getdev_cmd getdev_cmd;
	struct stdblockdev_devinfo_cmd devinfo;
	struct stdblockdev_res blockres;
	struct blockdev_info0 bdinf;

	if(working_threads[wpid].locked_logic_deviceid != logic_deviceid || working_threads[wpid].locked_deviceid != deviceid)
	{
		return FALSE;
	}

	// NOTE: using opened_files_mutex here would be wrong. for the device is already locked.
	// Besides calling leave_mutex(&opened_files_mutex) would produce secondary effects on close and open
	if(dinf->open_count <= 1 && dinf->dev_type == DEVTYPE_UNDEFINED)
	{
		
		working_threads[wpid].locked_device = 1;

		// get stddevport using stdservice query interface
		query_cmd.command = STDSERVICE_QUERYINTERFACE;
		query_cmd.uiid = STDDEV_UIID;
		query_cmd.ret_port = OFS_STDDEVRES_PORT;
		query_cmd.ver = STDDEV_VERSION;
		query_cmd.msg_id = wpid;

		locking_send_msg(deviceid, STDSERVICE_PORT, &query_cmd, wpid);
		
		get_signal_msg((int *)&query_res, wpid);

		if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
		{	
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_INTERFACE_NOT_FOUND);
			return FALSE;
		}

		dinf->stddev_port = query_res.port;

		// use stddev protocol

		// get device type //
		stddevcmd.command = STDDEV_GET_DEVICETYPE;
		stddevcmd.ret_port = OFS_STDDEVRES_PORT;
		stddevcmd.msg_id = wpid;
		
		locking_send_msg(deviceid, dinf->stddev_port, &stddevcmd, wpid);
		
		get_signal_msg((int *)&devtype_res, wpid);

		if(devtype_res.ret == STDDEV_ERR || devtype_res.dev_type == STDDEV_UNDEFINED)
		{
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
			return FALSE;
		}

		if(devtype_res.dev_type == STDDEV_BLOCK)
		{
			dinf->dev_type = DEVTYPE_BLOCK;
			
			if(devtype_res.block_size > OFS_BLOCK_SIZE || (OFS_BLOCK_SIZE % devtype_res.block_size != 0))
			{
				working_threads[wpid].locked_device = 0;
				*ret = build_response_msg(command, STDFSSERR_DEVICE_NOTSUPPORTED);
				return FALSE;
			}

			dinf->block_size = devtype_res.block_size;

			query_cmd.uiid = STD_BLOCK_DEV_UIID;
			query_cmd.ver = STD_BLOCK_DEV_VER;
		}
		else
		{
			dinf->dev_type = DEVTYPE_CHAR;

			query_cmd.uiid = STD_CHAR_DEV_UIID;
			query_cmd.ver = STD_CHAR_DEV_VER;
		}

		// issue query interface command //
		query_cmd.command = STDSERVICE_QUERYINTERFACE;
		query_cmd.ret_port = OFS_STDDEVRES_PORT;
		query_cmd.msg_id = wpid;

		locking_send_msg(deviceid, STDSERVICE_PORT, &query_cmd, wpid);
		
		get_signal_msg((int *)&query_res, wpid);

		if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
		{
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_INTERFACE_NOT_FOUND);
			return FALSE;
		}

		dinf->protocol_port = query_res.port;

		// if block device get device info
		if(dinf->dev_type == DEVTYPE_BLOCK)
		{
			// issue dev info
			devinfo.command = BLOCK_STDDEV_INFO;
			devinfo.dev = logic_deviceid;
			devinfo.devinf_smo = share_mem(deviceid, &bdinf, sizeof(struct blockdev_info0), WRITE_PERM);
			devinfo.devinf_id = DEVICEINFO_ID;
			devinfo.ret_port = OFS_STDDEVRES_PORT; // we will use stddev port for this.. even though it's a blockdev command.

			locking_send_msg(deviceid, dinf->protocol_port, &devinfo, wpid);
		
			get_signal_msg((int *)&blockres, wpid);
			
			claim_mem(devinfo.devinf_smo);

			if(blockres.ret == STDBLOCKDEV_ERR)
			{
				working_threads[wpid].locked_device = 0;
				*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
				return FALSE;
			}

			dinf->max_lba = bdinf.max_lba;
			dinf->ofst_lba = bdinf.metadata_lba_end;
		}

		// request device //
		getdev_cmd.command = ((request_mode & OFS_DEVICEREQUEST_EXCLUSIVE)? STDDEV_GET_DEVICEX : STDDEV_GET_DEVICE);
		getdev_cmd.ret_port = OFS_STDDEVRES_PORT;
		getdev_cmd.logic_deviceid = logic_deviceid;
		getdev_cmd.msg_id = wpid;

		locking_send_msg(deviceid, dinf->stddev_port, &getdev_cmd, wpid);
		
		get_signal_msg((int *)&stddevres, wpid);
		
		if(stddevres.ret == STDDEV_ERR)
		{
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
			return FALSE;
		}

		working_threads[wpid].locked_device = 0;

	}
	else if(dinf->open_count <= 1 && dinf->dev_type != DEVTYPE_UNDEFINED)
	{
		// this case will be when the device was kept cached 
		// but has no opened files referencing it.
		working_threads[wpid].locked_device = 1;
		getdev_cmd.command = ((request_mode & OFS_DEVICEREQUEST_EXCLUSIVE)? STDDEV_GET_DEVICEX : STDDEV_GET_DEVICE);
		getdev_cmd.ret_port = OFS_STDDEVRES_PORT;
		getdev_cmd.logic_deviceid = logic_deviceid;
		getdev_cmd.msg_id = wpid;

		locking_send_msg(deviceid, dinf->stddev_port, &getdev_cmd, wpid);
		
		get_signal_msg((int *)&stddevres, wpid);
		
		if(stddevres.ret == STDDEV_ERR)
		{
			leave_mutex(&opened_files_mutex);
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
			return FALSE;
		}
		working_threads[wpid].locked_device = 0;
	}
	
	return TRUE;
}

int free_device(int wpid, int deviceid, int logic_deviceid, struct sdevice_info *dinf, int command, struct stdfss_res **ret)
{
	struct stddev_res stddevres;
	struct stddev_freedev_cmd freedev_cmd;

	// device must be locked
	if(working_threads[wpid].locked_logic_deviceid != logic_deviceid || working_threads[wpid].locked_deviceid != deviceid)
	{
		return FALSE;
	}

	if(dinf->open_count == 0)
	{
		working_threads[wpid].locked_device = 1;

		// free the device
		freedev_cmd.command = STDDEV_FREE_DEVICE;
		freedev_cmd.ret_port = OFS_STDDEVRES_PORT;
		freedev_cmd.logic_deviceid = logic_deviceid;
		freedev_cmd.msg_id = wpid;
		
		locking_send_msg(deviceid, dinf->stddev_port, &freedev_cmd, wpid);
		
		get_signal_msg((int *)&stddevres, wpid);

		if(stddevres.ret == STDDEV_ERR)
		{
			working_threads[wpid].locked_device = 0;
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);

			return FALSE;
		}


		working_threads[wpid].locked_device = 0;
	}

	return TRUE;
}
int resolve_name(char *srvname, int *deviceid, int wpid)
{
	// This function is used when opening devices as files 
	// or mounting devices
	// it returns the device id for a given server name on the OS.

	struct directory_resolveid resolve;
	struct directory_response response;
	int i = 0;
	

	/*
		NOTE: Directory service resolution signals identify the thread
		based on the order of arrival of messages
	*/
	resolve.command = DIRECTORY_RESOLVEID;
	resolve.ret_port = OFS_DIRECTORY_PORT;
	resolve.service_name_smo = share_mem(DIRECTORY_TASK, srvname, len(srvname)+1, READ_PERM);

	wait_mutex(&max_directory_msg_mutex);
	{
		working_threads[wpid].directory_service_msg = max_directory_msg;
		max_directory_msg++;

		set_wait_for_signal(wpid, OFS_THREADSIGNAL_DIRECTORYMSG, -1);
	
		// send the msg inside the mutex to avoid someone picking a next greater but sending
		// the msg before we do
		send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &resolve);
	}
	leave_mutex(&max_directory_msg_mutex);

	wait_for_signal(wpid);

	wait_mutex(&max_directory_msg_mutex);
	{
		max_directory_msg--;
		// updates waiting order on wps
		while(i < OFS_MAXWORKINGTHREADS)
		{
			if(working_threads[i].directory_service_msg != -1)
			{
				working_threads[i].directory_service_msg--;
			}
			i++;
		}
	}
	leave_mutex(&max_directory_msg_mutex);
	
	claim_mem(resolve.service_name_smo);

	get_signal_msg((int*)&response, wpid);

	if(response.ret == DIRECTORYERR_NOTREGISTERED)
	{
		return FALSE;
	}

	*deviceid = response.ret_value;

	return TRUE;
}


void init_logic_device(device_info *logic_device, int devicefile_nodeid, int buffers)
{
	int i = 0;

	avl_init(&logic_device->buffers);
	avl_init(&logic_device->nodes);
	avl_init(&logic_device->procesing);
	init(&logic_device->waiting);
	logic_device->hits = 0;
	logic_device->blocked = FALSE;
	logic_device->dev_type = DEVTYPE_UNDEFINED;
	logic_device->mount_info = NULL;
	logic_device->device_nodeid = devicefile_nodeid;
	logic_device->open_count = 0;
	logic_device->umount = 0;

	init_mutex(&logic_device->processing_mutex);
	init_mutex(&logic_device->nodes_mutex);
	init_mutex(&logic_device->umount_mutex);
	init_mutex(&logic_device->blocked_mutex);

	if(buffers == TRUE)
	{
		while(i < OFS_NODEBUFFERS)
		{
			// allocate the node buffer
			logic_device->node_buffers[i] = (struct node_buffer*)malloc(sizeof(struct node_buffer));
			init_mutex(&logic_device->node_buffers[i]->buffer_mutex);
			logic_device->node_buffers[i]->first_node_id = -1;
			i++;
		}
	}
	else
	{
		while(i < OFS_NODEBUFFERS)
		{
			// allocate the node buffer
			logic_device->node_buffers[i] = NULL;
			i++;
		}
	}

	// st path cache in a known state, so it won't be freed if not initialized
	preinit_path_cache(&logic_device->pcache);
}

void free_device_struct(device_info *logic_device)
{
	int i= 0;

	// this functions frees memory used by a logic_device
	close_mutex(&logic_device->processing_mutex);
	close_mutex(&logic_device->nodes_mutex);
	close_mutex(&logic_device->umount_mutex);
	close_mutex(&logic_device->blocked_mutex);

	avl_remove_all(&logic_device->procesing);

	avl_remove_all(&logic_device->buffers);
	avl_remove_all(&logic_device->nodes);

	remove_all(&logic_device->waiting);

	while(i < OFS_NODEBUFFERS)
	{
		if(logic_device->node_buffers[i] != NULL)
		{
			close_mutex(&logic_device->node_buffers[i]->buffer_mutex);
			free(logic_device->node_buffers[i]);
			logic_device->node_buffers[i] = NULL;
		}
		i++;
	}

	if(logic_device->mount_info != NULL)
	{
		free_cache(&logic_device->pcache);
	}

	free(logic_device);
}
