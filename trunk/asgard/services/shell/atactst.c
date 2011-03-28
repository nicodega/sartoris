/*
*	Shell Service.
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

/*
*
*	This command is only ment to test char devices on ofs service.
*
*/


#include "shell_iternals.h"

#include <services/stds/stddev.h>
#include <services/stds/stdservice.h>
#include <services/stds/stdblockdev.h>
#include <services/atac/atac_ioctrl.h>

int stddevport = -1;
int protocol_port = -1;
int aid = 5;
void show_partition_list(int term);

char buffer[512];

void atactst(int term, char **args, int argc)
{
    int storage_size = 0, id, i;
    	
	struct stddev_devtype_res devtype_res;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	struct stddev_res stddevres;
	struct stddev_cmd stddevcmd;
	struct stdblockdev_read_cmd block_read;
	struct stdblockdev_res block_res;
	struct stddev_getdev_cmd getdev_cmd;

	term_color_print(term, "\nQuery Interface...  ", 7);

	// use stdservice query interface to get stddev port
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = SHELL_INITRET_PORT;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }

	get_msg(SHELL_INITRET_PORT, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
        term_color_print(term, "FAILED", 12);
		return;
	}

    term_color_print(term, "OK", 11);

	stddevport = query_res.port;

    term_color_print(term, "\nGet Device Type...  ", 7);
	// get device type //
	stddevcmd.command = STDDEV_GET_DEVICETYPE;
	stddevcmd.ret_port = SHELL_INITRET_PORT;
	stddevcmd.msg_id = 1;
	
	send_msg(aid, stddevport, &stddevcmd);
	
	while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }

	get_msg(SHELL_INITRET_PORT, &devtype_res, &id);

	if(devtype_res.dev_type != STDDEV_BLOCK)
	{
        term_color_print(term, "FAILED", 12);
		return;
	}

    term_color_print(term, "OK", 11);

	// issue query interface command for stdblockdev //
    term_color_print(term, "\nGet STD BLOCK DEV PORT...  ", 7);

	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STD_BLOCK_DEV_UIID;
	query_cmd.ret_port = SHELL_INITRET_PORT;
	query_cmd.ver = STD_BLOCK_DEV_VER;
	query_cmd.msg_id = 1;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);
	
	while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }

	get_msg(SHELL_INITRET_PORT, &query_res, &id);

	if(query_res.ret != STDSERVICE_RESPONSE_OK)
	{
        term_color_print(term, "FAILED", 12);
		return;
	}

	protocol_port = query_res.port;
	
    term_color_print(term, "OK", 11);

    // all set to comunicate with atac
    show_partition_list(term);
    term_color_print(term, "\n\n", 11);

    // request the device so we can read!
    term_color_print(term, "\nRequest device...  ", 7);

    getdev_cmd.command = STDDEV_GET_DEVICE;
	getdev_cmd.ret_port = SHELL_INITRET_PORT;
	getdev_cmd.logic_deviceid = 0;

	send_msg(aid, stddevport, &getdev_cmd);

	while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }
	
	get_msg(SHELL_INITRET_PORT, &stddevres, &id);

	if(stddevres.ret == STDDEV_ERR)
	{
		term_color_print(term, "FAILED", 12);
		return;
	}
    term_color_print(term, "OK", 11);
    term_color_print(term, "\nReading...  ", 7);

    // read sector 2 and show first dword
	block_read.buffer_smo = share_mem(aid, buffer, 512, WRITE_PERM);
	block_read.command = BLOCK_STDDEV_READ;
	block_read.dev = 0;
	block_read.msg_id = 0;
	block_read.pos = 1;
	block_read.ret_port = SHELL_INITRET_PORT;

    for(i = 0; i < 512; i++)
    {
        buffer[i] = 0xFF;
    }
    
	send_msg(aid, protocol_port, &block_read);

	while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }
	
	get_msg(SHELL_INITRET_PORT, &block_res, &id);

    claim_mem(block_read.buffer_smo);

    if(block_res.ret == STDBLOCKDEV_ERR)
	{
        term_color_print(term, "FAILED", 11);
    }
    else
    {
        term_color_print(term, "OK", 11);
        term_color_printf(term, "\nread %x %x %x \n", 11, *((int*)buffer), *((int*)&buffer[4]), *((int*)&buffer[8]));
    }

	return;
}


void show_partition_list(int term)
{
	/* Use IOCTRL command ATAC_IOCTRL_ENUMDEVS :D */	
	struct atac_enum_dev_param params;
	struct atac_ioctl_enumdevs cmd;
	struct stddev_ioctrl_res res;
	int taskid;
	struct fdisk_dev *dev = NULL;

    term_color_print(term, "\nListing Partitions...  ", 7);
	params.continuation = 0;

	cmd.command = STDDEV_IOCTL;
	cmd.request = ATAC_IOCTRL_ENUMDEVS;
	cmd.enum_dev_smo = share_mem(aid, &params, sizeof(struct atac_enum_dev_param), READ_PERM | WRITE_PERM);
	cmd.msg_id = 0;
	cmd.ret_port = SHELL_INITRET_PORT;
    
    params.devinf.size = 0;
    params.devinf.id = -1; 

	do
	{
		send_msg(aid, stddevport, &cmd);

		while(get_msg_count(SHELL_INITRET_PORT) == 0){ reschedule(); }

		get_msg(SHELL_INITRET_PORT, &res, &taskid);
		if(res.ret != STDDEV_ERR)
		{
            if(params.devinf.id & STDDEV_PHDEV_FLAG)
				term_color_printf(term, "\n-> P ldevid=%x, size=%i sec, metaend %i ",7, params.devinf.id, params.devinf.size, params.devinf.metadata_end);
			else
				term_color_printf(term, "\n  +- phyid=%i, ldevid=%i, slba=%i, size=%i sec, type=%x, metaend %i ", 7, params.devinf.pid, params.devinf.id, params.devinf.start_lba, params.devinf.size, params.devinf.ptype, params.devinf.metadata_end);
		}
	}while(res.ret != STDDEV_ERR);

	claim_mem(cmd.enum_dev_smo);
}

