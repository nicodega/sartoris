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

struct stdfss_res *mount_device(int wpid, struct working_thread *thread, struct stdfss_mount *mount_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL, *checkminf = NULL;
	struct sdevice_info *dinf = NULL;
	struct gc_node *devfile_node = NULL, *target_node = NULL, *rootn = NULL;
	struct stask_file_info *finf = NULL;
	struct stdblockdev_read_cmd block_read;
	struct stdblockdev_readm_cmd mblock_read;
	struct stdblockdev_write_cmd block_write;
	struct stdblockdev_res block_res;
	char *bits = NULL;
	int write_smo, read_smo;
	int dir_entries = 0;
	list mount_structures;
	char *str = NULL, *strmatched = NULL, *devfile_name = NULL, *devstrmatched = NULL;
	int logic_deviceid, deviceid, parse_ret;
	unsigned int dev_nodeid = (unsigned int)-1;
	CPOSITION it = NULL;
	unsigned int i, j;

	// mount device will lock the device file, making
	// all other operations on that file impossible 
	// NOTE: if there where more than one device file for a device
	// it could be mounted twice, or even opened as a file
	// while mounting
	// UPDATE: I don't know if this can really happen for the device
	// itself will be locked too. But when I wrote the note 
	// there was no device locking mechanism.

	/* DEVFILE RESOLUTION */

	// check mount path is not already mounted
	str = get_string(mount_cmd->mount_path);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	// get mount info for the device file
	devfile_name = get_string(mount_cmd->dev_path);

	ret = check_path(devfile_name, thread->command.command);
	if(ret != NULL)
	{
		free(str);
		return ret;
	}

	wait_mutex(&mounted_mutex);
	checkminf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(checkminf != NULL && len(strmatched) == len(str)) // check if there is something mounted on the path
	{
		free(str);
		free(devfile_name);
		free(strmatched);
		return build_response_msg(thread->command.command, STDFSSERR_PATH_INUSE);
	}
	else if(checkminf == NULL) // check mount path is mounted
	{
		// mount path does not exist
		free(str);
		free(strmatched);
		free(devfile_name);
		return build_response_msg(thread->command.command, STDFSSERR_NOT_FOUND);
	}

	// checkl mount path is not a prefix of the devfile path
	if(istrprefix(0, str, devfile_name))
	{
		free(str);
		free(strmatched);
		free(devfile_name);

		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// check mount path is a directory and it has no contents
	// Path will be locked as a FILE and not a directory
	// because latr we'll parse for device file, and that would 
	// unlock the node.
	parse_ret = parse_directory(FALSE, &target_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, checkminf, str, len(strmatched), NULL, NULL, &dir_entries, NULL, &ret);

	if(!parse_ret)
	{
		// directory does not exist
		free(str);
		free(strmatched);
		free(devfile_name);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_DOESNOTEXIST);
	}

	// is it a directory?
	if(target_node->n.type != OFS_DIRECTORY_FILE)
	{
		free(str);
		free(strmatched);
		free(devfile_name);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		nfree(checkminf->dinf, target_node);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	nfree(checkminf->dinf, target_node);
		
	if(dir_entries != 0)
	{
		free(str);
		free(strmatched);
		free(devfile_name);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_DIRNOTEMPTY);
	}

	// check device file path is mounted
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, devfile_name, &devstrmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_DOESNOTEXIST);
	}
	
	// parse for dev_file
	// here we lock the device as a directory
	// because the FILE lock is being used for the mount dir.
	parse_ret = parse_directory(FALSE, &devfile_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, thread->command.command, thread, wpid, minf, devfile_name, len(devstrmatched), NULL, &dev_nodeid, NULL, NULL, &ret);

	if(!parse_ret)
	{
		// dev file does not exists
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

		return build_response_msg(thread->command.command, STDFSSERR_FILE_DOESNOTEXIST);
	}

	// check it's a device file
	if(!(devfile_node->n.type & OFS_DEVICE_FILE))
	{
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);

		nfree(checkminf->dinf, devfile_node);

		return build_response_msg(thread->command.command, STDFSSERR_BAD_DEVICE_FILE);
	}

	
	// check device file is not opened on exclusive or write
	wait_mutex(&opened_files_mutex);
	{
		it = get_head_position(&opened_files);

		while(it != NULL)
		{
			finf = (struct stask_file_info *)get_next(&it);
			if(finf->dinf->mount_info != NULL && finf->dinf->device_nodeid == dev_nodeid)
			{
				if(finf->mode & STDFSS_FILEMODE_EXCLUSIVE || finf->mode & STDFSS_FILEMODE_WRITE)
				{
					leave_mutex(&opened_files_mutex);
					
					free(str);
					free(strmatched);
					free(devstrmatched);
					free(devfile_name);

					unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
					unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
			
					nfree(checkminf->dinf, devfile_node);

					return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
				}
				
				break;
			}
		}
	}
	leave_mutex(&opened_files_mutex);

	// get device info from file
	if(!read_device_file(devfile_node, minf, wpid, thread->command.command, &deviceid, &logic_deviceid, &ret))
	{
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);

		nfree(checkminf->dinf, devfile_node);

		return ret;
	}
	
	nfree(checkminf->dinf, devfile_node);

	// lock the device being mounted
	lock_device(wpid, deviceid, logic_deviceid);
	
	// free the device file node, for it's no longer necesary to keep it locked
	// as the device is locked :)
	unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);

	// see if the device is cached
	wait_mutex(&opened_files_mutex);
	{
		dinf = get_cache_device_info(deviceid, logic_deviceid);
		remove_cached_deviceE(deviceid, logic_deviceid); // remove the device from cache
	}
	leave_mutex(&opened_files_mutex);

	
	// check device is not already mounted
	wait_mutex(&mounted_mutex);
	{
		mount_structures = lpt_getvalues(mounted);
	
		it = get_head_position(&mount_structures);

		while(it != NULL)
		{
			minf = (struct smount_info *)get_next(&it);
			if(minf->deviceid == deviceid && minf->logic_deviceid == logic_deviceid)
			{
				leave_mutex(&mounted_mutex);

				// fail, for the device is already mounted
				unlock_device(wpid);
				
				free(str);
				free(strmatched);
				free(devstrmatched);
				free(devfile_name);
				lstclear(&mount_structures);
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

				return build_response_msg(thread->command.command, STDFSSERR_DEVICE_MOUNTED);
			}
		}

		lstclear(&mount_structures);
		
	}
	leave_mutex(&mounted_mutex);

	// check it's not opened as a file
	wait_mutex(&opened_files_mutex);
	{
		it = get_head_position(&opened_files);

		while(it != NULL)
		{
			finf = (struct stask_file_info *)get_next(&it);
			if(finf->dinf->mount_info == NULL && finf->deviceid == deviceid && finf->logic_deviceid == logic_deviceid)
			{
				leave_mutex(&opened_files_mutex);

				// fail, for the device is opened as a file
				unlock_device(wpid);
				
				free(str);
				free(strmatched);
				free(devstrmatched);
				free(devfile_name);
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

				return build_response_msg(thread->command.command, STDFSSERR_DEVICE_INUSE);
			}
		}
	}
	leave_mutex(&opened_files_mutex);
	
	if(dinf == NULL)
	{
		// init devinfo struct
		dinf = (struct sdevice_info *)malloc(sizeof(struct sdevice_info));
		init_logic_device(dinf, dev_nodeid, TRUE);
	}
	else
	{
		i = 0;

		// the device was probably opened as a file before
		// and that's why it's cached. Initialize node buffers
		while(i < OFS_NODEBUFFERS)
		{
			// allocate the node buffer
			dinf->node_buffers[i] = (struct node_buffer*)malloc(sizeof(struct node_buffer));
			init_mutex(&dinf->node_buffers[i]->buffer_mutex);
			dinf->node_buffers[i]->first_node_id = -1;
			i++;
		}
	}

	// request device
	if(!request_device(wpid, deviceid, logic_deviceid, dinf, ((mount_cmd->mode & STDFSS_MOUNTMODE_DEDICATED)? OFS_DEVICEREQUEST_EXCLUSIVE : 0), thread->command.command, &ret))
	{
		// fail, for the device is already mounted					
		unlock_device(wpid);
		
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

		return ret;
	}

	// check it's a block device
	if(!(dinf->dev_type & DEVTYPE_BLOCK) || dinf->block_size != OFS_BLOCKDEV_BLOCKSIZE)
	{
		free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

		// fail, for the device is already mounted					
		unlock_device(wpid);

		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}
	
	// MOUNT! //
	
	// switch thread working device to the one being mounted
	// so messages are not discarded
	
	thread->mounting = 1;

	// create mount structure for the path //
	minf = (struct smount_info *)malloc(sizeof(struct smount_info));
	minf->logic_deviceid = logic_deviceid;
	minf->mode = mount_cmd->mode;
	minf->deviceid = deviceid;
	minf->nodes_modified = 0;
	minf->blocks_modified = 0;
	minf->mode = mount_cmd->mode;

	// set device mount info to minf
	minf->dinf = dinf;
	minf->dinf->mount_info = minf;

	// clear the dir buffer
	clear_dir_buffer(thread->directory_buffer.buffer);

	// read the OFST from the device //
	write_smo = share_mem(deviceid, thread->directory_buffer.buffer, OFS_BLOCKDEV_BLOCKSIZE, WRITE_PERM);
	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = logic_deviceid;
	block_read.msg_id = wpid;
	block_read.pos = dinf->ofst_lba;
	block_read.ret_port = OFS_BLOCKDEV_PORT;

	locking_send_msg(deviceid, dinf->protocol_port, &block_read, wpid);

	get_signal_msg((int *)&block_res, wpid);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);

		thread->mounting = 0;

		free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

		// fail, for the device is already mounted					
		unlock_device(wpid);

		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		free(minf);
		
		claim_mem(write_smo);

		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

		return ret;
	}
	
	// copy write_smo data to the ofst //
	mem_copy(thread->directory_buffer.buffer, (unsigned char *)&minf->ofst, sizeof(struct OFST));
	
	// check 4K block size //
	if(minf->ofst.block_size != 4096 || minf->ofst.ptrs_on_node != PTRS_ON_NODE)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FORMAT_NOT_SUPPORTED);

		thread->mounting = 0;

		free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

		// fail, for the device is already mounted					
		unlock_device(wpid);

		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		free(minf);
		
		claim_mem(write_smo);

		// restore device for cleanup
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

		return ret;
	}
	
	// modify mount count - TODO: check of mount count = 0 (?)
	minf->ofst.mount_count++;

	// copy OFST back to the buffer //
	mem_copy((unsigned char *)&minf->ofst, thread->directory_buffer.buffer, sizeof(struct OFST));
	
	// write OFST to device //
	read_smo = share_mem(deviceid, thread->directory_buffer.buffer, 512, READ_PERM);
	block_write.buffer_smo = read_smo;
	block_write.command = BLOCK_STDDEV_WRITE;
	block_write.dev = logic_deviceid;
	block_write.msg_id = wpid;
	block_write.pos = dinf->ofst_lba;
	block_write.ret_port = OFS_BLOCKDEV_PORT;

	locking_send_msg(deviceid, dinf->protocol_port, &block_write, wpid);

	get_signal_msg((int *)&block_res, wpid);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
		
		thread->mounting = 0;

		free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

		// fail, for the device is already mounted					
		unlock_device(wpid);

		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		free(minf);
		
		claim_mem(write_smo);
		claim_mem(read_smo);

		// restore device for cleanup
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		
		return ret;
	}


	minf->group_size = OFS_BLOCKDEV_BLOCKSIZE + bitmaps_size(minf->ofst.nodes_per_group, minf->ofst.blocks_per_group) + OFS_BLOCKDEV_BLOCKSIZE * (minf->ofst.nodes_per_group / OFS_NODESPER_BLOCKDEV_BLOCK) + OFS_BLOCK_SIZE * minf->ofst.blocks_per_group;

	// read group headers //
	minf->group_headers = (struct GH *)malloc(sizeof(struct GH) * minf->ofst.group_count);
	
	i = 0;

	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = logic_deviceid;
	block_read.msg_id = wpid;	
	block_read.ret_port = OFS_BLOCKDEV_PORT;
	block_read.pos = minf->ofst.first_group;

	mblock_read.command = BLOCK_STDDEV_READM;
	mblock_read.dev = logic_deviceid;
	mblock_read.msg_id = wpid;	
	mblock_read.ret_port = OFS_BLOCKDEV_PORT;

	minf->group_m_free_blocks = (bitmap*)malloc(sizeof(bitmap) * minf->ofst.group_count);
	minf->group_m_free_nodes = (bitmap*)malloc(sizeof(bitmap) * minf->ofst.group_count);

	i = 0;
	while(i != minf->ofst.group_count)
	{
		minf->group_m_free_blocks[i].bits = NULL;
		minf->group_m_free_nodes[i].bits = NULL;
		i++;
	}

	i = 0;

	// there should be at least one group in the partition //
	while(i != minf->ofst.group_count)
	{
		locking_send_msg(deviceid, dinf->protocol_port, &block_read, wpid);

		get_signal_msg((int *)&block_res, wpid);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
			
			thread->mounting = 0;

			free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

			// fail, for the device is already mounted					
			unlock_device(wpid);
			
			free(str);
			free(strmatched);
			free(devstrmatched);
			free(devfile_name);

			free_device_struct(dinf);
			free(minf);
			
			claim_mem(write_smo);
			claim_mem(read_smo);
			
			j = 0;
			while(j != minf->ofst.group_count)
			{
				if(minf->group_m_free_blocks[j].bits != NULL)
				{
					free(minf->group_m_free_blocks[j].bits);
				}
				if(minf->group_m_free_nodes[j].bits != NULL)
				{
					free(minf->group_m_free_nodes[j].bits);
				}
				j++;
			}
			free(minf->group_m_free_blocks);
			free(minf->group_m_free_nodes);
			free(minf->group_headers);
			
			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
			
			return ret;
		}
		
		// suceeded => copy from buffer to group header //
		mem_copy(thread->directory_buffer.buffer, (unsigned char *)&minf->group_headers[i], sizeof(struct GH));
		
		// check for data consistency //
		
		if((minf->group_size != minf->group_headers[i].group_size) || (minf->ofst.blocks_per_group != minf->group_headers[i].blocks_per_group) || (minf->ofst.nodes_per_group != minf->group_headers[i].nodes_per_group))
		{
			ret = build_response_msg(thread->command.command, STDFSSERR_FORMAT_NOT_SUPPORTED);
			
			thread->mounting = 0;

			free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

			// fail, for the device is already mounted					
			unlock_device(wpid);

			free(str);
			free(strmatched);
			free(devstrmatched);
			free(devfile_name);

			free_device_struct(dinf);
			free(minf);
			
			claim_mem(write_smo);
			claim_mem(read_smo);

			int j = 0;
			while(j != minf->ofst.group_count)
			{
				if(minf->group_m_free_blocks[j].bits != NULL)
				{
					free(minf->group_m_free_blocks[j].bits);
				}
				if(minf->group_m_free_nodes[j].bits != NULL)
				{
					free(minf->group_m_free_nodes[j].bits);
				}
				j++;
			}

			
			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
			
			return ret;
		}
		
		// read group free nodes array
		bits = (char*)malloc(bitmaps_size(minf->group_headers[i].nodes_per_group, 0));
		mblock_read.buffer_smo = share_mem(deviceid, bits, bitmaps_size(minf->group_headers[i].nodes_per_group, 0), WRITE_PERM);
		mblock_read.pos = minf->group_headers[i].nodes_bitmap_offset;
		mblock_read.count = ((int)(minf->group_headers[i].nodes_per_group / BITS_PER_DEVBLOCK) + ((minf->group_headers[i].nodes_per_group % BITS_PER_DEVBLOCK == 0)? 0 : 1));

		locking_send_msg(deviceid, dinf->protocol_port, &mblock_read, wpid);

		get_signal_msg((int *)&block_res, wpid);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
			
			thread->mounting = 0;

			free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

			// fail, for the device is already mounted					
			unlock_device(wpid);
			
			free(str);
			free(strmatched);
			free(devstrmatched);
			free(devfile_name);

			free_device_struct(dinf);
			free(minf);
			
			claim_mem(write_smo);
			claim_mem(read_smo);

			j = 0;
			while(j != minf->ofst.group_count)
			{
				if(minf->group_m_free_blocks[j].bits != NULL)
				{
					free(minf->group_m_free_blocks[j].bits);
				}
				if(minf->group_m_free_nodes[j].bits != NULL)
				{
					free(minf->group_m_free_nodes[j].bits);
				}
				j++;
			}
			
			free(minf->group_m_free_blocks);
			free(minf->group_m_free_nodes);
			free(minf->group_headers);
			claim_mem(mblock_read.buffer_smo);

			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

			return ret;
		}

		
		claim_mem(mblock_read.buffer_smo);

		// setup bitmap
		init_bitmaparray(&minf->group_m_free_nodes[i], bits, minf->group_headers[i].nodes_per_group, bitmaps_size(minf->group_headers[i].nodes_per_group, 0) / OFS_BLOCKDEV_BLOCKSIZE, minf->ofst.nodes_per_group * i, 1);
		
		// read group m_free blocks array
		bits = (char*)malloc(bitmaps_size(0, minf->group_headers[i].blocks_per_group));
		mblock_read.buffer_smo = share_mem(deviceid, bits, bitmaps_size(0, minf->group_headers[i].blocks_per_group), WRITE_PERM);
		mblock_read.pos = minf->group_headers[i].blocks_bitmap_offset;
		mblock_read.count = ((int)(minf->group_headers[i].blocks_per_group / BITS_PER_DEVBLOCK) + ((minf->group_headers[i].blocks_per_group % BITS_PER_DEVBLOCK == 0)? 0 : 1));

		locking_send_msg(deviceid, dinf->protocol_port, &mblock_read, wpid);

		get_signal_msg((int *)&block_res, wpid);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
			
			thread->mounting = 0;

			free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

			// fail, for the device is already mounted					
			unlock_device(wpid);
			
			free(str);
			free(strmatched);
			free(devstrmatched);
			free(devfile_name);

			free_device_struct(dinf);
			free(minf);
			
			claim_mem(write_smo);
			claim_mem(read_smo);

			j = 0;
			while(j != minf->ofst.group_count)
			{
				if(minf->group_m_free_blocks[j].bits != NULL)
				{
					free(minf->group_m_free_blocks[j].bits);
				}
				if(minf->group_m_free_nodes[j].bits != NULL)
				{
					free(minf->group_m_free_nodes[j].bits);
				}
				j++;
			}
			free(minf->group_m_free_blocks);
			free(minf->group_m_free_nodes);
			free(minf->group_headers);
			claim_mem(mblock_read.buffer_smo);

			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

			return ret;
		}

		claim_mem(mblock_read.buffer_smo);

		// setup bitmap
		init_bitmaparray(&minf->group_m_free_blocks[i], bits, minf->group_headers[i].blocks_per_group, bitmaps_size(0, minf->group_headers[i].blocks_per_group) / OFS_BLOCKDEV_BLOCKSIZE, minf->group_headers[i].blocks_offset, OFS_BLOCKDEVBLOCK_SIZE);
		
		// switch pos to the next group header //
		block_read.pos = block_read.pos + minf->group_size;
		
		i++;
	}

	// get the base node of the partition //
	// (in a future we should cache all first level directory on a dir_cache structure for the given mounted structure) //
	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = logic_deviceid;
	block_read.msg_id = wpid;
	block_read.pos = minf->group_headers[0].nodes_table_offset;
	block_read.ret_port = OFS_BLOCKDEV_PORT;

	locking_send_msg(deviceid, dinf->protocol_port, &block_read, wpid);

	get_signal_msg((int *)&block_res, wpid);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
		
		thread->mounting = 0;

		free_device(wpid, deviceid, logic_deviceid, dinf, thread->command.command, &ret);

		// fail, for the device is already mounted					
		unlock_device(wpid);
		
		free(str);
		free(strmatched);
		free(devstrmatched);
		free(devfile_name);

		free_device_struct(dinf);
		free(minf);
		
		claim_mem(write_smo);
		claim_mem(read_smo);

		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

		return ret;
	}
	
	rootn = (struct gc_node*)malloc(sizeof(struct gc_node));
	rootn->ref = 1;
	rootn->nodeid = 0;

	mem_copy(thread->directory_buffer.buffer, (unsigned char *)&rootn->n, sizeof(struct node));
	
	avl_insert(&minf->dinf->nodes, rootn, 0);
	
	claim_mem(write_smo);
	claim_mem(read_smo);
	
#ifdef OFS_PATHCACHE
	init_path_cache(&minf->dinf->pcache, minf->dinf, str);
#endif

	wait_mutex(&mounted_mutex);
	lpt_insert(&mounted, str, minf);
	leave_mutex(&mounted_mutex);

	thread->mounting = 0;
	
	// unlock the device
	unlock_device(wpid);
	
	//free(str);
	free(strmatched);
	free(devstrmatched);
	free(devfile_name);

	init_mutex(&minf->free_space_mutex);

	// unlock mounted path
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_DENYALL);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
