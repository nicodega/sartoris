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

void init_ofs(struct stdfss_init *init_cmd, struct stdfss_res **ret)
{
	char *str;
	int id;
	unsigned int i, j;
	struct stddev_cmd stddevcmd;
	struct stddev_res stddevres;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	struct stddev_devtype_res devtype_res;
	struct smount_info *minf = NULL;
	struct stdblockdev_read_cmd block_read;
	struct stdblockdev_readm_cmd mblock_read;
	struct stdblockdev_write_cmd block_write;
	struct stddev_getdev_cmd getdev_cmd;
	struct stdblockdev_res block_res;
	struct stdblockdev_devinfo_cmd devinfo;
	struct blockdev_info0 bdinf;
	struct gc_node *rootn;
	unsigned char *buffer; // was an array, but we can use a directory buffer of the first wp.
	int read_smo, write_smo, stddev_port;
	char *bits = NULL;
	struct stddev_freedev_cmd freedevice_cmd;
	
	freedevice_cmd.command = STDDEV_FREE_DEVICE;
	freedevice_cmd.ret_port = OFS_BLOCKDEV_PORT;
	freedevice_cmd.msg_id = 0;
	freedevice_cmd.logic_deviceid = init_cmd->logic_deviceid;

	buffer = working_threads[0].directory_buffer.buffer;

	/*	As a device file is needed to mount every device, this
	*	message provides a way to mount the root of the ofs
	*	(note: this may be not the root of the entire filesystem)
	*	Params are used as follows:
	*		- smo to mount string.
	*		- service_name: the name of the device service on wich root will be mounted
	*/
	
	str = get_string(init_cmd->path_smo);

	*ret = check_path(str, init_cmd->command);
	if(*ret != NULL) return;

	if(initialized || lpt_getvalue(mounted, str) != NULL)
	{
		// root already mounted, generate message //
		*ret = build_response_msg(init_cmd->command, STDFSSERR_SERVICE_ALREADY_INITIALIZED);
		free(str);
		return;
	}

	// query for STDDEV interface using stdservice

	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = OFS_BLOCKDEV_PORT;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(init_cmd->deviceid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){ reschedule(); }

	get_msg(OFS_BLOCKDEV_PORT, &query_res, &id);
    if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		free(str);
		return;
	}
	
	stddev_port = query_res.port;

	// check device type //
	stddevcmd.command = STDDEV_GET_DEVICETYPE;
	stddevcmd.ret_port = OFS_BLOCKDEV_PORT;

	send_msg(init_cmd->deviceid, stddev_port, &stddevcmd);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
	
	get_msg(OFS_BLOCKDEV_PORT, &devtype_res, &id);

	if(devtype_res.ret == STDDEV_ERR || devtype_res.dev_type != STDDEV_BLOCK  || devtype_res.block_size != OFS_BLOCKDEV_BLOCKSIZE)
	{
		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		free(str);
		return;
	}
    	
	// request device //
	getdev_cmd.command = STDDEV_GET_DEVICEX;
	getdev_cmd.ret_port = OFS_BLOCKDEV_PORT;
	getdev_cmd.logic_deviceid = init_cmd->logic_deviceid;

	send_msg(init_cmd->deviceid, stddev_port, &getdev_cmd);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){ reschedule(); }
	
	get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

	if(stddevres.ret == STDDEV_ERR)
	{
		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		free(str);
		return;
	}

	// query for stdblockdev //
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STD_BLOCK_DEV_UIID;
	query_cmd.ret_port = OFS_BLOCKDEV_PORT;
	query_cmd.ver = STD_BLOCK_DEV_VER;

	send_msg(init_cmd->deviceid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
	
	get_msg(OFS_BLOCKDEV_PORT, &query_res, &id);

	if(query_res.ret != STDSERVICE_RESPONSE_OK)
	{
		send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_INTERFACE_NOT_FOUND);
		free(str);
		return;
	}

	// get device info
	devinfo.command = BLOCK_STDDEV_INFO;
	devinfo.dev = init_cmd->logic_deviceid;
	devinfo.devinf_smo = share_mem(init_cmd->deviceid, &bdinf, sizeof(struct blockdev_info0), WRITE_PERM);
	devinfo.devinf_id = DEVICEINFO_ID;
	devinfo.ret_port = OFS_BLOCKDEV_PORT; 
	
	send_msg(init_cmd->deviceid, query_res.port, &devinfo);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
	
	get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);

	claim_mem(devinfo.devinf_smo);

	if(block_res.ret != STDBLOCKDEV_OK)
	{
        send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_INTERFACE_NOT_FOUND);
		free(str);
		return;
	}
    
	// create mount structure for the path //
	minf = (struct smount_info *)malloc(sizeof(struct smount_info));
	minf->logic_deviceid = init_cmd->logic_deviceid;
	minf->mode = STDFSS_MOUNTMODE_READ | STDFSS_MOUNTMODE_WRITE | STDFSS_MOUNTMODE_DEDICATED;
	minf->deviceid = init_cmd->deviceid;
	minf->nodes_modified = 0;
	minf->blocks_modified = 0;

	// init device info
	minf->dinf = (struct sdevice_info*)malloc(sizeof(struct sdevice_info));
	init_logic_device(minf->dinf, -1, TRUE);
	minf->dinf->dev_type = DEVTYPE_BLOCK;
	minf->dinf->mount_info = minf;
	minf->dinf->protocol_port = query_res.port;
	minf->dinf->stddev_port = stddev_port;
	minf->dinf->max_lba = bdinf.max_lba;
	minf->dinf->ofst_lba = bdinf.metadata_lba_end;

	// read the OFST from the device //
	write_smo = share_mem(init_cmd->deviceid, buffer, 512, WRITE_PERM);
	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = init_cmd->logic_deviceid;
	block_read.msg_id = 0;
	block_read.pos = bdinf.metadata_lba_end;
	block_read.ret_port = OFS_BLOCKDEV_PORT;

	send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &block_read);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){ reschedule(); }
	
	get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);
    
    if(block_res.ret == STDBLOCKDEV_ERR)
	{		
		send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		lpt_remove(&mounted, str); // remove it from the mounted tree
		free(str);
		free_device_struct(minf->dinf);
		free(minf);
		claim_mem(write_smo);
		return;
	}
    

	// copy write_smo data to the ofst //
	mem_copy(buffer, (unsigned char *)&minf->ofst, sizeof(struct OFST));
	
	// check 4K block size and pointers on node //
	if(minf->ofst.block_size != 4096 || minf->ofst.ptrs_on_node != PTRS_ON_NODE || minf->ofst.Magic_number != OFS_MAGIC_NUMBER)
	{
        send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_FORMAT_NOT_SUPPORTED);
		free(str);
		free_device_struct(minf->dinf);
		free(minf);
		claim_mem(write_smo);
		return;
	}

	// modify mount count - TODO: check of mount count = 0 (?)
	minf->ofst.mount_count++;

	// copy OFST back to the buffer //
	mem_copy((unsigned char *)&minf->ofst, buffer, sizeof(struct OFST));

	// write OFST to device //
	read_smo = share_mem(init_cmd->deviceid, buffer, 512, READ_PERM);
	block_write.buffer_smo = read_smo;
	block_write.command = BLOCK_STDDEV_WRITE;
	block_write.dev = init_cmd->logic_deviceid;
	block_write.msg_id = 0;
	block_write.pos = bdinf.metadata_lba_end;
	block_write.ret_port = OFS_BLOCKDEV_PORT;

	send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &block_write);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

	get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);
    
    if(block_res.ret == STDBLOCKDEV_ERR)
	{
        send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		free(str);
		free_device_struct(minf->dinf);
		free(minf);
		claim_mem(write_smo);
		claim_mem(read_smo);
		return;
	}
	
	minf->group_size = OFS_BLOCKDEV_BLOCKSIZE + bitmaps_size(minf->ofst.nodes_per_group, minf->ofst.blocks_per_group) + OFS_BLOCKDEV_BLOCKSIZE * (minf->ofst.nodes_per_group / OFS_NODESPER_BLOCKDEV_BLOCK) + OFS_BLOCK_SIZE * minf->ofst.blocks_per_group;

	// read group headers //
	minf->group_headers = (struct GH *)malloc(sizeof(struct GH) * minf->ofst.group_count);
	
	i = 0;

	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = init_cmd->logic_deviceid;
	block_read.msg_id = 0;	
	block_read.ret_port = OFS_BLOCKDEV_PORT;
	block_read.pos = minf->ofst.first_group;

	
	mblock_read.command = BLOCK_STDDEV_READM;
	mblock_read.dev = init_cmd->logic_deviceid;
	mblock_read.msg_id = 1;	
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
		send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &block_read);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
		
		get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

			while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

			get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

			*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
			
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
			free(str);
			free_device_struct(minf->dinf);
			free(minf);
			claim_mem(write_smo);
			claim_mem(read_smo);
			return;
		}
		
		// suceeded => copy from buffer to group header //
		mem_copy(buffer, (unsigned char *)&minf->group_headers[i], sizeof(struct GH));
		
		// check for data consistency //
		
		if((minf->group_size != minf->group_headers[i].group_size) || (minf->ofst.blocks_per_group != minf->group_headers[i].blocks_per_group) || (minf->ofst.nodes_per_group != minf->group_headers[i].nodes_per_group))
		{
			send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

			while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

			get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

			*ret = build_response_msg(init_cmd->command, STDFSSERR_FORMAT_NOT_SUPPORTED);
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
			free(minf->group_m_free_blocks);
			free(minf->group_m_free_nodes);
			free(minf->group_headers);
			free(str);
			free_device_struct(minf->dinf);
			free(minf);
			claim_mem(write_smo);
			claim_mem(read_smo);
			return;
		}
		
		// read group free nodes array
		bits = (char*)malloc(bitmaps_size(minf->group_headers[i].nodes_per_group, 0));
		mblock_read.buffer_smo = share_mem(init_cmd->deviceid, bits, bitmaps_size(minf->group_headers[i].nodes_per_group, 0), WRITE_PERM);
		mblock_read.pos = minf->group_headers[i].nodes_bitmap_offset;
		mblock_read.count = ((int)(minf->group_headers[i].nodes_per_group / BITS_PER_DEVBLOCK) + ((minf->group_headers[i].nodes_per_group % BITS_PER_DEVBLOCK == 0)? 0 : 1));

		send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &mblock_read);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
		
		get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

			while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

			get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

			*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
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
			free(str);
			free_device_struct(minf->dinf);
			free(minf);
			claim_mem(write_smo);
			claim_mem(read_smo);
			claim_mem(mblock_read.buffer_smo);
			return;
		}

		
		claim_mem(mblock_read.buffer_smo);

		// setup bitmap
		init_bitmaparray(&minf->group_m_free_nodes[i], bits, minf->group_headers[i].nodes_per_group, bitmaps_size(minf->group_headers[i].nodes_per_group, 0) / OFS_BLOCKDEV_BLOCKSIZE, minf->ofst.nodes_per_group * i, 1);
		
		// read group m_free blocks array
		bits = (char*)malloc(bitmaps_size(0, minf->group_headers[i].blocks_per_group));
		mblock_read.buffer_smo = share_mem(init_cmd->deviceid, bits, bitmaps_size(0, minf->group_headers[i].blocks_per_group), WRITE_PERM);
		mblock_read.pos = minf->group_headers[i].blocks_bitmap_offset;
		mblock_read.count = ((int)(minf->group_headers[i].blocks_per_group / BITS_PER_DEVBLOCK) + ((minf->group_headers[i].blocks_per_group % BITS_PER_DEVBLOCK == 0)? 0 : 1));

		send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &mblock_read);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
		
		get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);

		if(block_res.ret == STDBLOCKDEV_ERR)
		{
			send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

			while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

			get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

			*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
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
			free(str);
			free_device_struct(minf->dinf);
			free(minf);
			claim_mem(write_smo);
			claim_mem(read_smo);
			claim_mem(mblock_read.buffer_smo);
			return;
		}

		claim_mem(mblock_read.buffer_smo);

		// setup bitmap
		init_bitmaparray(&minf->group_m_free_blocks[i], bits, minf->group_headers[i].blocks_per_group, bitmaps_size(0, minf->group_headers[i].blocks_per_group) / OFS_BLOCKDEV_BLOCKSIZE, minf->group_headers[i].blocks_offset, OFS_BLOCKDEVBLOCK_SIZE);
		
		// switch pos to the next group header //
		block_read.pos = block_read.pos + minf->group_size;
		
		i++;
	}

	// get the base node of the partition //
	// (in the future we should cache all first level directory on a dir_cache structure for the given mounted structure) //
	block_read.buffer_smo = write_smo;
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = init_cmd->logic_deviceid;
	block_read.msg_id = 0;
	block_read.pos = minf->group_headers[0].nodes_table_offset;
	block_read.ret_port = OFS_BLOCKDEV_PORT;

	send_msg(init_cmd->deviceid, minf->dinf->protocol_port, &block_read);

	while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}
	
	get_msg(OFS_BLOCKDEV_PORT, &block_res, &id);

	if(block_res.ret == STDBLOCKDEV_ERR)
	{
		send_msg(init_cmd->deviceid, stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_BLOCKDEV_PORT) == 0){reschedule();}

		get_msg(OFS_BLOCKDEV_PORT, &stddevres, &id);

		*ret = build_response_msg(init_cmd->command, STDFSSERR_DEVICE_ERROR);
		free(str);
		free_device_struct(minf->dinf);
		free(minf);
		claim_mem(write_smo);
		claim_mem(read_smo);
		return;
	}
	
	/* Insert root node on gc */
	rootn = (struct gc_node*)malloc(sizeof(struct gc_node));
	rootn->ref = 1;
	rootn->nodeid = 0;

	mem_copy(buffer, (unsigned char *)&rootn->n, sizeof(struct node));
	
	avl_insert(&minf->dinf->nodes, rootn, 0);
#ifdef OFS_PATHCACHE
	init_path_cache(&minf->dinf->pcache, minf->dinf, str);
#endif
	claim_mem(write_smo);
	claim_mem(read_smo);
	
	init_mutex(&minf->free_space_mutex);

	lpt_insert(&mounted, str, minf);

	*ret = build_response_msg(init_cmd->command, STDFSSERR_OK);

	initialized = 1;

	clear_dir_buffer(buffer);

	return;
}
