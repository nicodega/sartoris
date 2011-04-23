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

#include "atac.h"

char mallocbuffer[1024 * 32];	// 32kb

int dma_man_task = -1;

struct ata_channel *channelwp_channel = NULL;

/* Main service loop */
void _start()
{
	int die = 0, id;
	struct stdservice_res servres;
	struct stdservice_cmd service_cmd;
	struct stddev_cmd stddev_msg;
	struct directory_resolveid resolve_cmd;
	char *service_name = "devices/atac";
	struct directory_register reg_cmd;
	struct directory_response dir_res;

    // open ports with permisions for services only (lv 0, 1, 2) //
    open_port(1, 2, PRIV_LEVEL_ONLY);
    open_port(ATAC_SIGNALS_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDDEV_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDDEV_BLOCK_DEV_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(ATAC_THREAD_ACK_PORT, 2, PRIV_LEVEL_ONLY);
    
    __asm__ __volatile__ ("sti"::);

    /* Register with directory */
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = 1;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 13, READ_PERM);
		
    while(send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd) < 0)
    {
        reschedule(); // directory has not opened it's port yet
    }
        
	while (get_msg_count(1) == 0) { reschedule(); }

	get_msg(1, &dir_res, &id);

	claim_mem(reg_cmd.service_name_smo);
	
	/* Resolve DMA with directory */
	service_name = "system/dmac";
	
	resolve_cmd.command = DIRECTORY_RESOLVEID;
	resolve_cmd.ret_port = 1;
	resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);
	resolve_cmd.thr_id = get_current_thread();
        
	do
	{
		send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &resolve_cmd);

        while (get_msg_count(1) == 0){ reschedule(); }

		get_msg(1, &dir_res, &id);

		if(dir_res.ret != DIRECTORYERR_OK)
			dma_man_task = -1;
		else
			dma_man_task = dir_res.ret_value;
	}while(dma_man_task == -1);
   
    close_port(1);
    claim_mem(resolve_cmd.service_name_smo);

	dma_man_task = dir_res.ret_value;

	/* Continue initialization */
	init_mem(mallocbuffer, 1024 * 32);
	init_signals();
	set_signals_port(ATAC_SIGNALS_PORT);

	/* Find ata devices on system */
	ata_find();
    int k = 0;
	/* Command processing queue */
	while(!die)
	{
        while (get_msg_count(STDSERVICE_PORT) == 0 
			&& get_msg_count(STDDEV_PORT) == 0 
			&& get_msg_count(STDDEV_BLOCK_DEV_PORT) == 0 
			&& get_msg_count(ATAC_THREAD_ACK_PORT) == 0)
		{ 			
			string_print("ATAC ALIVE",160 - 20,k++);
		    reschedule(); 
		}

		while(get_msg_count(ATAC_THREAD_ACK_PORT) > 0)
		{
			struct pm_msg_response msg_res;

			get_msg(ATAC_THREAD_ACK_PORT, &msg_res, &id);

			/* Generate an event :) */
			send_event(get_current_task(), THREAD_CREATED_EVENT, msg_res.req_id, 0, (msg_res.status == PM_THREAD_OK), 0);
		}

		/* Process std service messages */
		int service_count = get_msg_count(STDSERVICE_PORT);
		
		while(service_count != 0)
		{
			get_msg(STDSERVICE_PORT, &service_cmd, &id);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				for(;;); // FIXME
				break;
			}
			else if(service_cmd.command == STDSERVICE_QUERYINTERFACE)
			{
				process_query_interface((struct stdservice_query_interface *)&service_cmd, id);
				service_count--;
				continue;
			}
			send_msg(id, service_cmd.ret_port, &servres);
			service_count--;
		}

		/* Process stddev messages */
		int stddev_count = get_msg_count(STDDEV_PORT); 

		while(!die && stddev_count != 0)
		{
			get_msg(STDDEV_PORT, &stddev_msg, &id);

			struct stddev_res res = process_stddev(stddev_msg, id);

			send_msg(id, stddev_msg.ret_port, &res);

			stddev_count--;
		}

		/* Process stdblockdev messages */
		int stdblockdev_count = get_msg_count(STDDEV_BLOCK_DEV_PORT);
		struct stdblockdev_cmd block_msg;
		struct stdblockdev_res block_res;

		while(!die && stdblockdev_count != 0)
		{			
			get_msg(STDDEV_BLOCK_DEV_PORT, &block_msg, &id);

			// check ownership for command processing
			if(!process_blockdev(&block_msg, id, &block_res))
			{
				send_msg(id, block_msg.ret_port, &block_res);
			}
			
			stdblockdev_count--;
		}

	}

}
