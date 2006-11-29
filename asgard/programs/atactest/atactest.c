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

#include <lib/dirlib.h>
#include <lib/printf.h>
#include <lib/debug.h>

#include <services/stds/stddev.h>
#include <services/stds/stdservice.h>
#include <services/stds/stdblockdev.h>

char sector_buffer[512];

int main(int argc, char **argv) 
{
	// let's test the ata controler service :D
	int storage_size = 0, protocol_port, id;

	printf("resolving ata controler id... ");

	int aid = dir_resolveid("devices/atac");
	
	printf("ata controler id=%i\nsending query interface fo stddev...", aid);

	struct stddev_devtype_res devtype_res;
	struct stdblockdev_devinfo_cmd devinfo;
	struct stdblockdev_res stdblock_res;
	struct blockdev_info0 dinf;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	int logic_deviceid = 0;	// first partition on channel 0, controler 0
	struct stddev_res stddevres;

	// use stdservice query interface to get stddev port
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = 7;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		return 1;
	}

	int port = query_res.port;

	printf("protocol port=%i\nsending get device type...", port);

	struct stddev_cmd stddevcmd;

	// get device type //
	stddevcmd.command = STDDEV_GET_DEVICETYPE;
	stddevcmd.ret_port = 7;
	stddevcmd.msg_id = 1;
	
	send_msg(aid, port, &stddevcmd);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &devtype_res, &id);

	if(devtype_res.dev_type != STDDEV_BLOCK)
	{
		return 1;
	}

	printf("devices is tdblockdev\nsending query interface for stdblockdev...");

	// issue query interface command for stdblockdev //
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STD_BLOCK_DEV_UIID;
	query_cmd.ret_port = 7;
	query_cmd.ver = STD_BLOCK_DEV_VER;
	query_cmd.msg_id = 1;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &query_res, &id);

	if(query_res.ret != STDSERVICE_RESPONSE_OK)
	{
		return 1;
	}

	protocol_port = query_res.port;

	printf("protocol port=%i\nsending get device info...", protocol_port);

	// get storage device size
	devinfo.command = BLOCK_STDDEV_INFO;
	devinfo.dev = logic_deviceid;
	devinfo.devinf_smo = share_mem(aid, &dinf, sizeof(struct blockdev_info0), WRITE_PERM);
	devinfo.devinf_id = DEVICEINFO_ID;
	devinfo.ret_port = 7;

	send_msg(aid, protocol_port, &devinfo);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &stdblock_res, &id);

	claim_mem(devinfo.devinf_smo);

	if(stdblock_res.ret != STDBLOCKDEV_OK) return 1;

	storage_size = dinf.media_size;

	printf("media size=%x\nsending request device...", storage_size);

	struct stddev_getdev_cmd getdev_cmd;

	getdev_cmd.command = STDDEV_GET_DEVICE;
	getdev_cmd.ret_port = 7;
	getdev_cmd.logic_deviceid = logic_deviceid;
	getdev_cmd.msg_id = 0;

	send_msg(aid, port, &getdev_cmd);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &stddevres, &id);

	if(stddevres.ret != STDDEV_OK) return 1;

	printf("device ownership granted!\nwriting sector lba 1 [0x34567890]...");

	struct stdblockdev_write_cmd bwrite;

	bwrite.command = BLOCK_STDDEV_WRITE;
	bwrite.dev = logic_deviceid;			// the logic device ID
	bwrite.buffer_smo = share_mem(aid, sector_buffer, 512, READ_PERM);;
	bwrite.pos = 1;
	bwrite.msg_id = 0;		// used for non ordered protocol
	bwrite.ret_port = 7;

	unsigned int *i = (unsigned int*)sector_buffer;

	*i = 0x34567890;

	send_msg(aid, protocol_port, &bwrite);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &stdblock_res, &id);

	claim_mem(bwrite.buffer_smo);

	if(stdblock_res.ret != STDBLOCKDEV_OK) return 1;

	printf("OK\nreading lba 1... ");

	*i = 0;

	struct stdblockdev_read_cmd bread;

	bread.command = BLOCK_STDDEV_READ;
	bread.dev = logic_deviceid;			// the logic device ID
	bread.buffer_smo = share_mem(aid, sector_buffer, 512, WRITE_PERM);;
	bread.pos = 1;
	bread.msg_id = 0;		// used for non ordered protocol
	bread.ret_port = 7;

	send_msg(aid, protocol_port, &bread);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &stdblock_res, &id);

	claim_mem(bwrite.buffer_smo);

	if(stdblock_res.ret != STDBLOCKDEV_OK) return 1;

	printf("OK, read=%x", *i);	
	
	return 0;
} 
	
