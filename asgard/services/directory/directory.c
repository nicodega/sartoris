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

#include "directory_internals.h"
#include <lib/wait_msg_sync.h>

AvlTree services_by_id;
lpat_tree services_by_name;

char dmbuffer[1024 * 10]; // 10kb for malloc

char *get_string(int smo);

void directory_main()
{	
	struct directory_cmd command;
	struct servdirectory_entry *entry;
	struct directory_response response;
	struct stdservice_cmd service_cmd;
	struct stdservice_res servres;
	int *services_to_remove = (int *)NULL;
	int id, die = 0, services_total;
	char *resolving_service, service_count;
	struct stdservice_res dieres;
	int dieid;
	int dieretport, i=0;
    char *name;

    // open the port with permisions for services only (lv 0, 1, 2) //
	open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(DIRECTORY_PORT, 2, PRIV_LEVEL_ONLY);

	// set interrupts
	__asm__ __volatile__ ("sti"::);
	
    init_mem(dmbuffer, 1024 * 10);

	avl_init(&services_by_id);
	lpt_init(&services_by_name);

    int ports[] = {STDSERVICE_PORT, DIRECTORY_PORT};
    int counts[2];
    unsigned int mask = 0x3;

	while(!die)
	{
		while(wait_for_msgs_masked(ports, counts, 2, mask) == 0){}

		service_count = counts[0];
		
		while(service_count != 0)
		{            
			get_msg(STDSERVICE_PORT, &service_cmd, &id);
            __asm__ __volatile__ ("cli"::);
            print("DIR DIE from task %i ", id);
            for(;;);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				die = 1;
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				dieid = id;
				dieretport = service_cmd.ret_port;
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

		while(!die && counts[1])
		{
			get_msg(DIRECTORY_PORT, &command, &id);

			response.command = command.command;
			response.thr_id = command.thr_id;
			response.ret = DIRECTORYERR_OK;
            
			switch(command.command)
			{
			    case DIRECTORY_REGISTER_SERVICE:
				    // see if it's already registered
				    entry = (struct servdirectory_entry *)avl_getvalue(services_by_id, id);

				    if(entry != NULL)
				    {		
					    /*
						    NOTE: In a future, we could check with samurai if this is the
						    same service who registered before and if so update its
						    id on the entry. (and fix the trees)
					    */
					    response.ret = DIRECTORYERR_ALREADYREGISTERED;

					    break;
				    }

                    name = get_string(((struct directory_register *)&command)->service_name_smo);

                    if(name == NULL)
                    {
                        print("DIR Invalid SMO %i, task %i ", ((struct directory_register *)&command)->service_name_smo, id);
                        response.ret = DIRECTORYERR_INVALID_SMO;
					    break;
                    }
                    
				    entry = (struct servdirectory_entry *)malloc(sizeof(struct servdirectory_entry));
				    entry->service_name = name;
				    entry->serviceid = id;

				    avl_insert(&services_by_id, entry, id);
				    lpt_insert(&services_by_name, entry->service_name, entry);

				    break;
			    case DIRECTORY_UNREGISTER_SERVICE:
				    /*
					    NOTE: again, here we should check if it really is the same service
					    or someone impersonated it (cheating is baaad :) )
				    */

				    entry = (struct servdirectory_entry *)avl_getvalue(services_by_id, id);

				    if(entry == NULL)
				    {		
					    /*
						    NOTE: In a future, we could check with samurai if this is the
						    same service who registered before and if so update its
						    id on the entry. (and fix the trees)
					    */
					    response.ret = DIRECTORYERR_NOTREGISTERED;

					    break;
				    }

				    avl_remove(&services_by_id, id);
				    lpt_remove(&services_by_name, entry->service_name);

				    free(entry->service_name);
				    free(entry);

				    break;
			    case DIRECTORY_RESOLVEID:

				    resolving_service = get_string(((struct directory_resolveid *)&command)->service_name_smo);

                    if(resolving_service == NULL)
                    {
                        print("DIR Invalid SMO %i, task %i ", ((struct directory_register *)&command)->service_name_smo, id);
                        response.ret = DIRECTORYERR_INVALID_SMO;
					    break;
                    }

				    entry = (struct servdirectory_entry *)lpt_getvalue(services_by_name, resolving_service);
	
				    if(entry == NULL)
				    {
					    response.ret = DIRECTORYERR_NOTREGISTERED;
					    free(resolving_service);
					    break;
				    }
				    response.ret_value = entry->serviceid;
				    free(resolving_service);

				    break;
			    case DIRECTORY_RESOLVENAME:

				    entry = (struct servdirectory_entry *)avl_getvalue(services_by_id, ((struct directory_resolvename *)&command)->serviceid);

				    if(entry == NULL)
				    {
					    response.ret = DIRECTORYERR_NOTREGISTERED;
					    break;
				    }

				    // write on name smo
				    int size = mem_size(((struct directory_resolvename *)&command)->name_smo);

				    if(len(entry->service_name) > size)
				    {
					    response.ret = DIRECTORYERR_SMO_TOOSMALL;
					    response.ret_value = size;
				    }
				    else
				    {
					    if(write_mem(((struct directory_resolvename *)&command)->name_smo, 0, size, entry->service_name) == FAILURE)
                        {
                            response.ret = DIRECTORYERR_INVALID_SMO;
					        break;
                        }
				    }

				    break;
			}

			send_msg(id, command.ret_port, &response);

            counts[1]--;
		}
	}

	// free all structures 
	services_total = avl_get_indexes(&services_by_id, &services_to_remove);

	if(services_total && services_to_remove != NULL)
	{
		while(services_total > 0)
		{
			entry = (struct servdirectory_entry *)avl_getvalue(services_by_id, services_to_remove[services_total - 1]);

			lpt_remove(&services_by_name, entry->service_name);
			avl_remove(&services_by_id, entry->serviceid);

			free(entry->service_name);
			free(entry);

			services_total--;
		}
	}

	send_msg(dieid, dieretport, &dieres);

	for(;;);
}

char *get_string(int smo)
{
	//get_string: gets a string from a Shared Memory Object
	int size = mem_size(smo);

    if(size <= 0) return NULL;

    char *tmp = (char *)malloc(size);

	if(tmp == NULL)
	{
		print("DIR: NULL MALLOC size %i ", size);
		for(;;);
	}
	if(read_mem(smo, 0, size, tmp))
	{
		return NULL;		
	}

	return tmp;
}

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
	struct stdservice_query_res qres;

	qres.command = STDSERVICE_QUERYINTERFACE;
	qres.ret = STDSERVICE_RESPONSE_FAILED;
	qres.msg_id = query_cmd->msg_id;

	switch(query_cmd->uiid)
	{
		case DIRECTORY_UIID:
			// check version
			if(query_cmd->ver != DIRECTORY_VER) break;
			qres.port = DIRECTORY_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}

