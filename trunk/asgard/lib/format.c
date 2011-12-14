/*
*
*	A simple format library unsing iolib. Currently it only supports a simple OFS Files system.
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

#include <services/ofsservice/ofs.h>
#include <lib/iolib.h>
#include <lib/scheduler.h>
#include <lib/format.h>
#include <lib/malloc.h>
#include <services/stds/stddev.h>
#include <services/stds/stdservice.h>
#include <services/stds/stdblockdev.h>

#include <lib/debug.h>

int bitmaps_size(int node_count, int block_count);

int format_port = FORMAT_PORT;

char bitmap_buff[OFS_BLOCKDEV_BLOCKSIZE];

int bitmaps_size(int node_count, int block_count);
int format_simple_ofs(char *device_file_name, void(*print_func)(char*,int));

void set_format_port(int port){format_port = port;}

int format(char *device_file_name, int type, void *params, int params_count, void(*print_func)(char*,int))
{

	switch(type)
	{
	case FORMAT_SIMPLE_OFS_TYPE:
		return format_simple_ofs(device_file_name, print_func);
		break;
	default:
		return 1;
	}

	return 1; // FAIL
}

int bitmaps_size(int node_count, int block_count)
{
	int nodes = ((int)(node_count / BITS_PER_DEVBLOCK) + ((node_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	int blocks = ((int)(block_count / BITS_PER_DEVBLOCK) + ((block_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	return nodes * OFS_BLOCKDEV_BLOCKSIZE + blocks * OFS_BLOCKDEV_BLOCKSIZE;
}

int format_simple_ofs(char *device_file_name, void(*print_func)(char*,int))
{
	// open the device for r/w
	FILE *devf = NULL;
	int count = 0, deviceid, logic_deviceid;
	int storage_size = 0, protocol_port, id;
	FILEINFO *finf = NULL;
	struct stddev_cmd stddevcmd;
	struct stddev_devtype_res devtype_res;
	struct stdblockdev_devinfo_cmd devinfo;
	struct stdblockdev_res stdblock_res;
	struct blockdev_info0 dinf;
	struct OFST ofst;
	struct GH gh;
	struct node rootn;
	int block_count, node_count, remaining, proposed;
	int lbabitmaps_end, i ,j, port;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	
	// fetch service_id and logic device id
	finf = finfo(device_file_name);

	if(finf == NULL) return 1;	

	deviceid = finf->device_service_id;
	logic_deviceid = finf->logic_device_id;

	free(finf);

	// use stddev protocol

	// use stdservice query interface to get stddev port
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = format_port;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(deviceid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(format_port) == 0){ reschedule(); }

	get_msg(format_port, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		return 1;
	}

	port = query_res.port;

	if(print_func != NULL) print_func("\nDevice Type check... ", 7);

	// get device type //
	stddevcmd.command = STDDEV_GET_DEVICETYPE;
	stddevcmd.ret_port = format_port;
	stddevcmd.msg_id = 1;
	
	send_msg(deviceid, port, &stddevcmd);
	
	while(get_msg_count(format_port) == 0){ reschedule(); }

	get_msg(format_port, &devtype_res, &id);

	if(devtype_res.dev_type != STDDEV_BLOCK)
	{
		if(print_func != NULL) print_func("NON BLOCK", 12);
		return 1;
	}

	if(print_func != NULL) print_func("BLOCK", 11);
	if(print_func != NULL) print_func("\nQuery interface for stddevblock... ", 7);

	// issue query interface command for stdblockdev //
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STD_BLOCK_DEV_UIID;
	query_cmd.ret_port = format_port;
	query_cmd.ver = STD_BLOCK_DEV_VER;
	query_cmd.msg_id = 1;

	send_msg(deviceid, STDSERVICE_PORT, &query_cmd);
	
	while(get_msg_count(format_port) == 0){ reschedule(); }

	get_msg(format_port, &query_res, &id);

	if(query_res.ret != STDSERVICE_RESPONSE_OK)
	{
		if(print_func != NULL) print_func("FAILED", 12);
		return 1;
	}

	if(print_func != NULL) print_func("OK", 11);

	protocol_port = query_res.port;

	// get storage device size
	devinfo.command = BLOCK_STDDEV_INFO;
	devinfo.dev = logic_deviceid;
	devinfo.devinf_smo = share_mem(deviceid, &dinf, sizeof(struct blockdev_info0), WRITE_PERM);
	devinfo.devinf_id = DEVICEINFO_ID;
	devinfo.ret_port = format_port;

	send_msg(deviceid, protocol_port, &devinfo);
	
	while(get_msg_count(format_port) == 0){ reschedule(); }

	get_msg(format_port, &stdblock_res, &id);

	if(stdblock_res.ret != STDBLOCKDEV_OK) return 1;

	storage_size = dinf.media_size;

	if(print_func != NULL) print_func("\nCalculating nodes/blocks... ", 7);

	// create OFS Table
	ofst.first_group = 1 + dinf.metadata_lba_end;
	ofst.group_count = 1;
	ofst.mount_count = 0;
	ofst.block_size = OFS_BLOCK_SIZE;
	ofst.Magic_number = OFS_MAGIC_NUMBER;
	ofst.MetaData = OFS_NULL_VALUE;
	ofst.ptrs_on_node = PTRS_ON_NODE;
	ofst.node_size = 128;

	unsigned int skip_bytes = dinf.metadata_lba_end * OFS_BLOCKDEV_BLOCKSIZE;

	// calculate block and node count based on space required for headers, bitmaps, etc
	block_count = 0;
	node_count = 0;
	remaining = storage_size - OFS_BLOCKDEV_BLOCKSIZE * (2 + dinf.metadata_lba_end);

	// maximize block and node count 
	proposed = OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
	proposed += bitmaps_size(node_count + OFS_NODESPER_BLOCKDEV_BLOCK, block_count + OFS_NODESPER_BLOCKDEV_BLOCK);

	// atempt to assign an equal ammount of nodes and blocks, if not possible assign more blocks
	while(remaining - proposed >= 0)
	{
		block_count += OFS_NODESPER_BLOCKDEV_BLOCK;
		node_count += OFS_NODESPER_BLOCKDEV_BLOCK;

		remaining -= OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
		proposed = OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
		proposed += bitmaps_size(node_count + OFS_NODESPER_BLOCKDEV_BLOCK, block_count + OFS_NODESPER_BLOCKDEV_BLOCK);
	}

	// fill remaining space with blocks //
	proposed = OFS_BLOCK_SIZE + bitmaps_size(node_count, block_count + 1);

	while(remaining - proposed > 0)
	{
		block_count++;
		proposed = OFS_BLOCK_SIZE + bitmaps_size(node_count, block_count + 1);
		remaining -= OFS_BLOCK_SIZE;
	}

	if(print_func != NULL) print_func("DONE", 11);

	j = 0;

	ofst.nodes_per_group = node_count;
	ofst.blocks_per_group = block_count;

	if(print_func != NULL) print_func("\nOpening Device... ", 7);

	// write tables and nodes
	devf = fopen(device_file_name, "wb");

	// create OFS disk with one group //
	if(devf == NULL)
	{
		if(print_func != NULL) print_func("FAILED", 12);
		return 1; // FAIL
	}

	if(print_func != NULL) print_func("OK", 11);
	if(print_func != NULL) print_func("\nWriting OFST... ", 7);

	if(fseek(devf, skip_bytes, SEEK_SET))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	// write ofst to media
	if(fwrite(&ofst, 1, sizeof(struct OFST), devf) != sizeof(struct OFST))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	// calculate block and node co
    print("OFST %i %i %i %i %x %i %i %i %i \n", ofst.first_group, ofst.group_count, ofst.mount_count, ofst.block_size, ofst.Magic_number, ofst.ptrs_on_node, ofst.node_size, ofst.nodes_per_group, ofst.blocks_per_group);

	if(print_func != NULL) print_func("OK", 11);

	// create group header
	gh.blocks_bitmap_offset = 2 + dinf.metadata_lba_end;
	gh.nodes_bitmap_offset = 2 + dinf.metadata_lba_end + bitmaps_size(0, block_count) / OFS_BLOCKDEV_BLOCKSIZE;
	gh.blocks_per_group = block_count;
	gh.nodes_per_group = node_count;
	gh.group_id = 0;
	gh.group_size = OFS_BLOCKDEV_BLOCKSIZE + bitmaps_size(node_count, block_count) + OFS_BLOCKDEV_BLOCKSIZE * (node_count / OFS_NODESPER_BLOCKDEV_BLOCK) + OFS_BLOCK_SIZE * block_count;
	gh.meta_data = OFS_NULL_VALUE;
	gh.meta_data_size = 0;
	gh.nodes_table_offset = gh.nodes_bitmap_offset + bitmaps_size(node_count, 0) / OFS_BLOCKDEV_BLOCKSIZE;
	gh.blocks_offset = gh.nodes_bitmap_offset + bitmaps_size(node_count, 0)  / OFS_BLOCKDEV_BLOCKSIZE + (int)(node_count / OFS_NODESPER_BLOCKDEV_BLOCK) + ((node_count % OFS_NODESPER_BLOCKDEV_BLOCK == 0)? 0 : 1);
	
    print("GH %x %x %i %i %i %i %x %x\n", gh.blocks_bitmap_offset, gh.nodes_bitmap_offset, gh.blocks_per_group, gh.nodes_per_group, gh.group_id, gh.group_size, gh.nodes_table_offset, gh.blocks_offset);

	// write Group Header to media

	if(print_func != NULL) print_func("\nWriting Group Header... ", 7);

	if(fseek(devf, OFS_BLOCKDEV_BLOCKSIZE + skip_bytes, SEEK_SET))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	if(fwrite(&gh, 1, sizeof(struct GH), devf) != sizeof(struct GH))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	if(print_func != NULL) print_func("OK", 11);

	if(print_func != NULL) print_func("\nInitializing Bitmaps... ", 7);

	// initialize bitmaps
	if(fseek(devf, gh.blocks_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}
	
	i = gh.blocks_bitmap_offset;
	j = 0;
	lbabitmaps_end = bitmaps_size(node_count, block_count) / OFS_BLOCKDEV_BLOCKSIZE + gh.blocks_bitmap_offset;
	
	// init zeroing buffer
	while(j < OFS_BLOCKDEV_BLOCKSIZE)
	{
		bitmap_buff[j] = 0;
		j++;
	}

	i++;

	while(i < lbabitmaps_end)
	{
		if(fwrite(bitmap_buff, 1, OFS_BLOCKDEV_BLOCKSIZE, devf) != OFS_BLOCKDEV_BLOCKSIZE)
		{	
			if(print_func != NULL) print_func("FAILED", 12);
			fclose(devf);
			return 1;
		}
		i++;
	}

	if(fseek(devf, gh.nodes_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	if(print_func != NULL) print_func("OK", 11);

	if(print_func != NULL) print_func("\nCreating Root Node... ", 7);

	bitmap_buff[0] = 0x80; // set first node as used for it's the root dir
	
	if(fwrite(bitmap_buff, 1, OFS_BLOCKDEV_BLOCKSIZE, devf) != OFS_BLOCKDEV_BLOCKSIZE)
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}
	// create root node
	i = PTRS_ON_NODE - 1;
	while(i >= 0)
	{
		rootn.blocks[i] = OFS_NULL_VALUE;
		i--;
	}

	rootn.file_size = 0;
	rootn.link_count = 0;
	rootn.next_node = OFS_NULL_VALUE;
	rootn.type = OFS_DIRECTORY_FILE;
	rootn.creation_date = 0;
	rootn.modification_date = 0;
	rootn.owner_group_id = 0;
	rootn.owner_id = 0;
	rootn.protection_mode = OFS_PROTECTIONMODE_OUR | OFS_PROTECTIONMODE_OGR | OFS_PROTECTIONMODE_OTR;

	// write the node
	if(fseek(devf, gh.nodes_table_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET)) 
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	if(fwrite(&rootn, 1, sizeof(struct node), devf) != sizeof(struct node))
	{	
		if(print_func != NULL) print_func("FAILED", 12);
		fclose(devf);
		return 1;
	}

	fclose(devf);

	if(print_func != NULL) print_func("OK", 11);

	return 0; // ok
}
