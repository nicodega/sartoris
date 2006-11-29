/*
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

#include "ramdisk_internals.h"

#include <lib/debug.h>
#include <lib/iolib.h>
#include <lib/printf.h>
#include <services/ofsservice/ofs.h>

struct stdservice_res dieres;
int dieid;
int dieretport;

static int owners = 0;
static int exclusive = -1;
static int ownerid[MAX_OWNERS] = {-1,}; // array of device owners

static unsigned int drive_size = 0;

char buffer[DISK_SIZE];

void mem_copy(unsigned char *source, unsigned char *dest, int count)
{
	int i = count;
	
	while(i != 0){
		dest[i] = source[i];
		i--;
	}
}

/* functions */

int main(int argc, char **args) 
{
	int die = 0, id_proc = -1, result, i = 0, res;
	struct thread thr;
	struct stddev_cmd stddev_msg;
	struct stdblockdev_cmd rdsk_msg;
	struct stdblockdev_res rdsk_res;
	struct stdservice_cmd service_cmd;
	int service_count, stddev_count, stdblockdev_count;
	char *service_namef = "devices/ramdisk%i";
	char service_name[256];
	struct directory_register reg_cmd;
	struct directory_response dir_res;
	struct blockdev_info0 devinf;
	struct stdblockdev_devinfo_cmd *dinfmsg;
	struct stdservice_res servres;


	// get file info and load the file
	unsigned int instance = 0;

	// remember drive size must be a 512 multiple, if not... make it so
	
	if(argc > 2)
	{
		printf("Invalid parameters.");		
		return 1;
	}
	else if(argc == 2 && !streq(args[1], "--blank"))
	{
		FILEINFO *finf = finfo(args[1]);

		if(finf == NULL)
		{
			printf("Could not get file size.");		
			return 1;
		}

		unsigned int fsize = (unsigned int)finf->file_size;

		free(finf);
		
		if(fsize > DISK_SIZE)
		{
			printf("File is too big.");		
			return 1;
		}

		FILE *fp = fopen(args[1], "r");

		if(fp == NULL)
		{
			printf("Could not open file");		
			return 1;
		}

		unsigned int read = 0;
		
		while(read < fsize && !feof(fp))
		{
			unsigned int tread = ((fsize - read > 4096)? 4096 : fsize);
			if(fread(buffer + read, 1, tread, fp) != tread)
			{
				printf("Warning: could not read full file!\n");		
				break;
			}
			read += tread;
		}

		fclose(fp);

		drive_size = fsize + (fsize % 512);
	}
	else
	{
		// --blank option
		// create a floppy image 
		init_disk(buffer);
	}

	devinf.max_lba = drive_size / 512;
	devinf.media_size = drive_size;
	
	sprintf(service_name, service_namef, instance);

	// register process with directory //
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = 1;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd);

	while (get_msg_count(1) == 0) { reschedule(); }

	get_msg(1, &dir_res, &id_proc);

	claim_mem(reg_cmd.service_name_smo);

	/* now we enter an infinite loop, checking and procesing
	   messages */
	printf("Process will now stay resident...");	

	while(!die)
	{		
		while (get_msg_count(STDSERVICE_PORT) == 0 && get_msg_count(STDDEV_PORT) == 0 && get_msg_count(STDDEV_BLOCK_DEV_PORT) == 0) { reschedule(); }

		/* process stdservice commands */
		service_count = get_msg_count(STDSERVICE_PORT);
		
		while(service_count != 0)
		{
			get_msg(STDSERVICE_PORT, &service_cmd, &id_proc);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				die = 1;
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				dieid = id_proc;
				dieretport = service_cmd.ret_port;
				break;
			}
			else if(service_cmd.command == STDSERVICE_QUERYINTERFACE)
			{
				process_query_interface((struct stdservice_query_interface *)&service_cmd, id_proc);
				service_count--;
				continue;
			}
			send_msg(id_proc, service_cmd.ret_port, &servres);
			service_count--;
		}

		stddev_count = get_msg_count(STDDEV_PORT); 

		/* process stddev messages */
		while(!die && stddev_count != 0)
		{
			get_msg(STDDEV_PORT, &stddev_msg, &id_proc);

			struct stddev_res res = process_stddev(stddev_msg, id_proc);

			send_msg(id_proc, stddev_msg.ret_port, &res);

			stddev_count--;
		}

		stdblockdev_count = get_msg_count(STDDEV_BLOCK_DEV_PORT);

		/* process stdblock_dev msgs */
		while(!die && stdblockdev_count != 0)
		{			
			get_msg(STDDEV_BLOCK_DEV_PORT, &rdsk_msg, &id_proc);

			rdsk_msg.dev &= !STDDEV_PHDEV_FLAG; // treat everything as a physical device

			// check ownership for command processing
			if(!die && rdsk_msg.command == BLOCK_STDDEV_INFO)
			{
				// fill struct
				dinfmsg = (struct stdblockdev_devinfo_cmd *)&rdsk_msg;
				write_mem(dinfmsg->devinf_smo, 0, sizeof(struct blockdev_info0), &devinf);

				// send ok response
				rdsk_res.command = rdsk_msg.command;
				rdsk_res.dev = (unsigned char)rdsk_msg.dev; 
				rdsk_res.msg_id = rdsk_msg.msg_id;
				rdsk_res.ret = STDBLOCKDEV_OK;
				send_msg(id_proc, rdsk_msg.ret_port, &rdsk_res);
				stdblockdev_count--;
				continue;
			}
			else if(!die && (!check_ownership(id_proc, rdsk_msg.dev) || rdsk_msg.dev != 0 ))
			{
				rdsk_res.command = rdsk_msg.command;
				rdsk_res.dev = (unsigned char)rdsk_msg.dev; 
				rdsk_res.msg_id = rdsk_msg.msg_id;
				rdsk_res.ret = STDBLOCKDEV_ERR;
				if(rdsk_msg.dev != 0)
				{
					rdsk_res.extended_error = STDBLOCKDEVERR_WRONG_LOGIC_DEVICE;
				}
				else
				{
					rdsk_res.extended_error = -1;
				}
				send_msg(id_proc, rdsk_msg.ret_port, &rdsk_res);
				stdblockdev_count--;
				continue;
			}

			
			// ramdisk will consider only pysical devices.
			switch (rdsk_msg.command) {
			case BLOCK_STDDEV_WRITE: 
				result = write_block(rdsk_msg.pos, rdsk_msg.buffer_smo);
				break;
			case BLOCK_STDDEV_READ: 
				result = read_block(rdsk_msg.pos, rdsk_msg.buffer_smo);
				break;
			case BLOCK_STDDEV_WRITEM: 
				result = mwrite_block((struct stdblockdev_writem_cmd*)&rdsk_msg);
				break;
			case BLOCK_STDDEV_READM: 
				result = mread_block((struct stdblockdev_readm_cmd*)&rdsk_msg);
				break;
			default:
				result = RAMDISK_ERR;
			}
			
			rdsk_res.command = rdsk_msg.command;
			rdsk_res.dev = (unsigned char)rdsk_msg.dev;
			rdsk_res.msg_id = rdsk_msg.msg_id;

			if(result != RAMDISK_OK)
			{
				rdsk_res.ret = STDBLOCKDEV_ERR;
			}
			else
			{
				rdsk_res.ret = STDBLOCKDEV_OK;
			}
			
			send_msg(id_proc, rdsk_msg.ret_port, &rdsk_res);
			
			stdblockdev_count--;
		}
		
	}

	/* tell the sender to unload us */
	send_msg(dieid, dieretport, &dieres);

	return 0;
}

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
	struct stdservice_query_res qres;

	qres.command = STDSERVICE_QUERYINTERFACE;
	qres.ret = STDSERVICE_RESPONSE_FAILED;
	qres.msg_id = query_cmd->msg_id;

	switch(query_cmd->uiid)
	{
		case STDDEV_UIID:
			// check version
			if(query_cmd->ver != STDDEV_VERSION) break;
			qres.port = STDDEV_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		case STD_BLOCK_DEV_UIID:
			if(query_cmd->ver != STD_BLOCK_DEV_VER) break;
			qres.port = STDDEV_BLOCK_DEV_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}
struct stddev_res process_stddev(struct stddev_cmd stddev_msg, int sender_id)
{
	struct stddev_res res;
	struct stddev_devtype_res *type_res;
	struct stddev_ver_res *ver_res;
	int i = 0;

	res.logic_deviceid = -1;
	res.command = stddev_msg.command;

	switch(stddev_msg.command)
	{
		case STDDEV_GET_DEVICETYPE:
			type_res = (struct stddev_devtype_res*)&res;
			type_res->dev_type = STDDEV_BLOCK;
			type_res->ret = STDDEV_OK;
			type_res->msg_id = stddev_msg.msg_id;
			type_res->block_size = 512;
			return *((struct stddev_res*)type_res);
		case STDDEV_VER:
			ver_res = (struct stddev_ver_res*)&res;
			ver_res->ret = STDDEV_OK;
			ver_res->ver = STDDEV_VERSION;
			ver_res->msg_id = stddev_msg.msg_id;
			return *((struct stddev_res*)ver_res);
		case STDDEV_GET_DEVICE:
			res.logic_deviceid = stddev_msg.padding0;
			if(exclusive == -1)
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
				ownerid[owners] = sender_id;
				owners++;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			break;
		case STDDEV_GET_DEVICEX:
			res.logic_deviceid = stddev_msg.padding0;
			if(owners == 0)
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
				ownerid[owners] = sender_id;
				owners++;
				exclusive = 0;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			break;
		case STDDEV_FREE_DEVICE:
			res.logic_deviceid = stddev_msg.padding0;
			if(exclusive == -1)
			{
				// non exclusive free
				while(i < owners)
				{
					if(ownerid[i] == sender_id)
					{
						res.msg_id = stddev_msg.msg_id;
						res.ret = STDDEV_OK;
						owners--;
						swap_array(ownerid, i, owners);
						return res;
					}
					i++;
				}
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			else
			{
				// exclusive free
				if(ownerid[0] == sender_id)
				{
					res.msg_id = stddev_msg.msg_id;
					res.ret = STDDEV_OK;
					owners--;
					exclusive = -1;
					return res;
				}
				res.ret = STDDEV_ERR;
			}
			break;
	}

	return res;
}

void swap_array(int *array, int start, int length)
{
	int k = start;

	while(start < length)
	{
		array[k] = array[k + 1];
		k++;
	}

	if(length != 0) array[k] = -1;
	
}

int check_ownership(int process_id, int logic_device)
{
	int i = 0;

	while(i < owners)
	{
		int ow = ownerid[i];
		if(ownerid[i] == process_id) return 1;
		i++;
	}

	return 0;
}

int read_block(int block, int smo_id_dest) {
	return ramdisk_rw(block, smo_id_dest, 0, 0);
}

int write_block(int block, int smo_id_src) {
	return ramdisk_rw(block, smo_id_src, 0, 1);
}
int mwrite_block(struct stdblockdev_writem_cmd *cmd)
{
	int count = 0, offset = 0;
	while(count < cmd->count)
	{
		if(ramdisk_rw(cmd->pos, cmd->buffer_smo, offset, 1) != RAMDISK_OK) return RAMDISK_ERR;
		offset += 512;
		count++;
		cmd->pos++;
	}
	return RAMDISK_OK; //ok
}
int mread_block(struct stdblockdev_readm_cmd *cmd)
{
	int count = 0, offset = 0;
	while(count < cmd->count)
	{
		if(ramdisk_rw(cmd->pos, cmd->buffer_smo, offset, 0) != RAMDISK_OK) return RAMDISK_ERR;
		offset += 512;
		count++;
		cmd->pos++;
	}
	return RAMDISK_OK; //ok
}



int ramdisk_rw(int block, int smo_id, int size, int write)
{
	if(block > RAMDISK_ERR)
		return drive_size;
	if(write)
	{
		if(read_mem(smo_id, 0, size, (char*)(buffer + block * 512)))
			return RAMDISK_ERR;
	}
	else
	{
		if(write_mem(smo_id, 0, size, (char*)(buffer + block * 512)))
			return RAMDISK_ERR;
	}

	return RAMDISK_OK;
} 

int bitmaps_size(int node_count, int block_count)
{
	int nodes = ((int)(node_count / BITS_PER_DEVBLOCK) + ((node_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	int blocks = ((int)(block_count / BITS_PER_DEVBLOCK) + ((block_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	return nodes * OFS_BLOCKDEV_BLOCKSIZE + blocks * OFS_BLOCKDEV_BLOCKSIZE;
}

void init_disk(char *disk)
{
	struct node rootn;

	int k = 0;

	// load blank disk structure //
	for(k = 0; k < DISK_SIZE; k++)
	{
		disk[k] = -1;
		k++;
	}

	// create OFS disk with one group //

	// create OFS Table
	struct OFST *ofst = (struct OFST *)disk;

	ofst->first_group = 1;
	ofst->group_count = 1;
	ofst->mount_count = 0;
	ofst->block_size = OFS_BLOCK_SIZE;
	ofst->Magic_number = OFS_MAGIC_NUMBER;
	ofst->MetaData = OFS_NULL_VALUE;
	ofst->ptrs_on_node = PTRS_ON_NODE;
	ofst->node_size = 128;

	// calculate block and node count based on space required for headers, bitmaps, etc
	int block_count = 0;
	int node_count = 0;
	int remaining = DISK_SIZE - OFS_BLOCKDEV_BLOCKSIZE * 2; // disk - OFST - GH

	// maximize block and node count
	int proposed = OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
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

	ofst->nodes_per_group = node_count;
	ofst->blocks_per_group = block_count;

	// create group header
	struct GH *gh = (struct GH *)(disk + OFS_BLOCKDEV_BLOCKSIZE);

	gh->blocks_bitmap_offset = 2;
	gh->nodes_bitmap_offset = 2 + bitmaps_size(0, block_count) / OFS_BLOCKDEV_BLOCKSIZE;
	gh->blocks_per_group = block_count;
	gh->nodes_per_group = node_count;
	gh->group_id = 0;
	gh->group_size = OFS_BLOCKDEV_BLOCKSIZE + bitmaps_size(node_count, block_count) + OFS_BLOCKDEV_BLOCKSIZE * (node_count / 
OFS_NODESPER_BLOCKDEV_BLOCK) + OFS_BLOCK_SIZE * block_count;
	gh->meta_data = OFS_NULL_VALUE;
	gh->meta_data_size = 0;
	gh->nodes_table_offset = gh->nodes_bitmap_offset + bitmaps_size(node_count, 0) / OFS_BLOCKDEV_BLOCKSIZE;
	gh->blocks_offset = gh->nodes_bitmap_offset + bitmaps_size(node_count, 0) / OFS_BLOCKDEV_BLOCKSIZE + (int)(node_count / OFS_NODESPER_BLOCKDEV_BLOCK) + ((node_count % OFS_NODESPER_BLOCKDEV_BLOCK == 0)? 0 : 1);

	// initialize bitmaps
	unsigned char *ptr = (unsigned char *) ((long)disk + (gh->blocks_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE));
	int i = gh->blocks_bitmap_offset, j = 0;
	int lbabitmaps_end = bitmaps_size(node_count, block_count) / OFS_BLOCKDEV_BLOCKSIZE + gh->blocks_bitmap_offset;
	while(i < lbabitmaps_end)
	{
		j = 0;
		while(j < OFS_BLOCKDEV_BLOCKSIZE)
		{
			ptr[j] = 0;
			j++;
		}
		ptr += OFS_BLOCKDEV_BLOCKSIZE;
		i++;
	}

	ptr = (unsigned char *)(disk + (gh->nodes_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE));

	*ptr = 0x80; //only if root alone is needed

	//*ptr = 0xC0; // reserve node 0 for root and 1 for a file

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

	/*rootn.blocks[0] = gh->blocks_offset; // assign first block for the root 
dir

	ptr = (unsigned char *)(disk + (gh->blocks_bitmap_offset * 
OFS_BLOCKDEV_BLOCKSIZE));

	*ptr = 0x80;
	*/
	*((struct node*)(disk + (gh->nodes_table_offset * OFS_BLOCKDEV_BLOCKSIZE))) = rootn;

}

