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

#include <os/pman_task.h>
#include <services/pci/pci.h>
#include <services/stds/stdservice.h>
#include <services/directory/directory.h>
#include <lib/structures/list.h>
#include <lib/structures/string.h>
#include <lib/malloc.h>
#include "pci.h"

#define PCI_PORT	0x2

char malloc_buffer[1024 * 30]; // 30 kb

list pci_devices;

/* Service entry point */
void _start()
{
	int id;
	struct stdservice_cmd service_cmd;
	struct stdservice_res servres;
	struct stdservice_res dieres;
	struct pci_cmd pcmd;
	char *service_name = "devices/pci";
	struct directory_register reg_cmd;
	struct directory_response dir_res;

	init_mem(malloc_buffer, 1024 * 30);
	init(&pci_devices);

    // open ports with permisions for services only (lv 0, 1, 2) //
	open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(PCI_PORT, 2, PRIV_LEVEL_ONLY);

	__asm__ ("sti" ::);

	// register with directory
	// register service with directory //
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = 1;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 11 + 1, READ_PERM);

	while (send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd) == -1) { reschedule(); }

	while (get_msg_count(1) == 0) { reschedule(); }

	get_msg(1, &dir_res, &id);

	claim_mem(reg_cmd.service_name_smo);

	/* enumerate PCI devices on the system */
	enum_pci();

	for(;;)
	{
		while(get_msg_count(PCI_PORT) == 0 && get_msg_count(STDSERVICE_PORT) == 0)
		{ 
			reschedule(); 
		}

		// process incoming STDSERVICE messages
		int service_count = get_msg_count(STDSERVICE_PORT);
			
		while(service_count != 0)
		{
			get_msg(STDSERVICE_PORT, &service_cmd, &id);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				// FIXME: return failure to al pending commands and die 
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				send_msg(id, service_cmd.ret_port, &dieres);  			
				for(;;);
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

		// process incoming pci messages
		int pci_count = get_msg_count(PCI_PORT);
			
		while(pci_count != 0)
		{
			get_msg(PCI_PORT, &pcmd, &id);
			process_pci_cmd(&pcmd, id);
			pci_count--;
		}
	}
}

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
	struct stdservice_query_res qres;

	qres.command = STDSERVICE_QUERYINTERFACE;
	qres.ret = STDSERVICE_RESPONSE_FAILED;
	qres.msg_id = query_cmd->msg_id;

	switch(query_cmd->uiid)
	{
		case PCI_UIID:
			// check version
			if(query_cmd->ver != PCI_VER) break;
			qres.port = PCI_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}

void process_pci_cmd(struct pci_cmd *pcmd, int task)
{
	struct pci_res *res = NULL;

	switch(pcmd->command)
	{
		case PCI_GETMAPPED_REGIONS:
			res = get_mapped_regions((struct pci_getmregions*)pcmd);
			break;
		case PCI_GETCONFIG:
			res = get_devices((struct pci_getdevices*)pcmd);
			break;
		case PCI_CONFIGURE:
			res = get_config((struct pci_getcfg*)pcmd);
			break;
		case PCI_GETDEVICES:
			res = set_config((struct pci_setcfg*)pcmd);
			break;
		default:
			res = build_response_msg(PCIERR_INVALID_COMMAND);
	}


	if(res != NULL)
	{
		res->thr_id = pcmd->thr_id;
		res->command = pcmd->command;

		send_msg(task, pcmd->ret_port, res);

		free(res);
	}
}

struct pci_res *build_response_msg(int ret)
{
	struct pci_res *res = (struct pci_res *)malloc(sizeof(struct pci_res));
	res->ret = ret;
	return res;
}

