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

#include "pipes_srv_internals.h"

char malloc_buffer[1024 * 30]; // 30 kb

AvlTree pipes;

/* Service entry point */
pipes_main()
{
	int id;
	struct stdservice_cmd service_cmd;
	struct stdservice_res servres;
	struct stdservice_res dieres;
	struct pipes_cmd pcmd;
	char *service_name = "fs/pipes";
	struct directory_register reg_cmd;
	struct directory_response dir_res;

	init_mem(malloc_buffer, 1024 * 30);
	avl_init(&pipes);

	_sti; // set interrupts

	// register with directory
	// register service with directory //
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = 1;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name) + 1, READ_PERM);
	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd);

	while (get_msg_count(1) == 0) { reschedule(); }

	get_msg(1, &dir_res, &id);

	claim_mem(reg_cmd.service_name_smo);

	for(;;)
	{
		while(get_msg_count(PIPES_PORT) == 0 && get_msg_count(STDSERVICE_PORT) == 0)
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

		// process incoming pipes messages
		int pipes_count = get_msg_count(PIPES_PORT);
			
		while(pipes_count != 0)
		{
			get_msg(PIPES_PORT, &pcmd, &id);
			process_pipes_cmd(&pcmd, id);
			pipes_count--;
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
		case PIPES_UIID:
			// check version
			if(query_cmd->ver != PIPES_VER) break;
			qres.port = PIPES_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}

void process_pipes_cmd(struct pipes_cmd *pcmd, int task)
{
	struct pipes_res *res= NULL;
	struct pipe *p = NULL;

	// check for open
	if(pcmd->command == PIPES_OPENSHARED)
	{
		// create a new shared pipe 
		struct pipe *p = (struct pipe*)malloc(sizeof(struct pipe));

		p->id = get_new_pipeid();
		p->type = PIPES_SHAREDPIPE;
		p->taskid = ((struct pipes_openshared*)pcmd)->task1;
		p->taskid2 = ((struct pipes_openshared*)pcmd)->task2;
		p->pf = NULL;
		p->creating_task = task;
		p->buffer = (struct pipe_buffer*)malloc(sizeof(struct pipe_buffer));
		p->buffer->rcursor = p->buffer->wcursor = p->buffer->size = 0;
		p->pending = 0;
		p->task1_closed = 0;
		p->task2_closed = 0;
		init(&p->buffer->blocks);

		avl_insert(&pipes, p, p->id);

		res = build_response_msg(PIPESERR_OK);
		((struct pipes_open_res*)res)->pipeid = p->id;
	}
	else if(pcmd->command == PIPES_OPENFILE)
	{
		// create a new file pipe 
		struct pipe *p = (struct pipe*)malloc(sizeof(struct pipe));

		char *filepath = get_string(((struct pipes_openfile*)pcmd)->path_smo);

		p->id = get_new_pipeid();
		p->type = PIPES_FILEPIPE;
		p->taskid = ((struct pipes_openshared*)pcmd)->task1;
		p->taskid2 = -1;
		p->pf = fopen(filepath, (char*)((struct pipes_openfile*)pcmd)->open_mode);
		p->creating_task = task;
		p->buffer = NULL;
		p->pending = 0;

		if(p->pf != NULL)
		{
			avl_insert(&pipes, p, p->id);
			res = build_response_msg(PIPESERR_OK);
			((struct pipes_open_res*)res)->pipeid = p->id;
		}
		else
		{
			res = build_response_msg(PIPESERR_FSERROR);
			free(p);
		}

		free(filepath);
	}
	else
	{
		p = (struct pipe*)avl_getvalue(pipes, ((struct pipes_close*)pcmd)->pipeid);

		if(p != NULL)
		{
			/* Check permissions */
			switch(pcmd->command)
			{
				case PIPES_CLOSE:
					// a shared pipe must be closed on both ends or by the creating task
					if(p->type == PIPES_SHAREDPIPE)
					{
						if(task != p->taskid && task != p->taskid2 && task != p->creating_task)
						{
							res = build_response_msg(PIPESERR_ERR);
						}
						else if((task == p->taskid && p->task1_closed) || (task == p->taskid2 && p->task2_closed))
						{
							res = build_response_msg(PIPESERR_PIPECLOSED);
						}
					}
					else if(p->type == PIPES_FILEPIPE && task != p->taskid)
					{
						res = build_response_msg(PIPESERR_ERR); 
					}
					break; 
				case PIPES_SEEK:
				case PIPES_TELL:	
					break;
				case PIPES_WRITE:
				case PIPES_PUTS:
				case PIPES_PUTC:
					if(p->type == PIPES_SHAREDPIPE && task != p->taskid)
					{
						res = build_response_msg(PIPESERR_ERR); 
					}
					break; 
				case PIPES_READ:
				case PIPES_GETS:
				case PIPES_GETC:
					if(p->type == PIPES_SHAREDPIPE && task != p->taskid2)
					{
						res = build_response_msg(PIPESERR_ERR); 
					}
					break; 
				default:
					res = build_response_msg(PIPESERR_ERR);
			}

			if(res != NULL)
			{
				res->thr_id = pcmd->thr_id;
				res->command = pcmd->command;

				send_msg(task, pcmd->ret_port, res);
				return;
			}

			/* Process pipe command */
			switch(pcmd->command)
			{
				case PIPES_CLOSE:
				res = pclose((struct pipes_close *)pcmd, p, task);
				if(res->ret == PIPESERR_OK) p = NULL;
					break;
				case PIPES_READ:
				res = pread((struct pipes_read *)pcmd, p);
					break;
				case PIPES_WRITE:
				res = pwrite((struct pipes_write *)pcmd, p);
					break;
				case PIPES_SEEK:
				res = pseek((struct pipes_seek *)pcmd, p, task);
					break;
				case PIPES_TELL:
				res = ptell((struct pipes_tell *)pcmd, p, task);
					break;
				case PIPES_PUTS:
				res = pputs((struct pipes_puts *)pcmd, p);
					break;
				case PIPES_PUTC:
				res = pputc((struct pipes_putc *)pcmd, p);
					break;
				case PIPES_GETS:
				res = pgets((struct pipes_gets *)pcmd, p);
					break;
				case PIPES_GETC:
				res = pgetc((struct pipes_getc *)pcmd, p);
					break;
				default:
				res = build_response_msg(PIPESERR_ERR);
			}
		}
		else
		{
			res = build_response_msg(PIPESERR_PIPECLOSED);
		}
	}
	
	if(p == NULL || (p != NULL && !(p->pending && (pcmd->command == PIPES_READ 
					|| (pcmd->command == PIPES_SEEK && task == p->taskid2)
					|| pcmd->command == PIPES_GETS 
					|| pcmd->command == PIPES_GETC))))
	{
		if(res == NULL)
		{
			res = build_response_msg(PIPESERR_ERR);
		}
		
		res->thr_id = pcmd->thr_id;
		res->command = pcmd->command;

		send_msg(task, pcmd->ret_port, res);

		// check pending read
		if(p != NULL && p->pending && (pcmd->command == PIPES_WRITE || pcmd->command == PIPES_PUTS || pcmd->command == PIPES_PUTC))
		{
			// process the pending message
			process_pending(p);
		}
	}

	if(res != NULL) free(res);
}

struct pipes_res *build_response_msg(int ret)
{
	struct pipes_res *res = (struct pipes_res *)malloc(sizeof(struct pipes_res));
	res->ret = ret;
	return res;
}

char *get_string(int smo)
{
	//get_string: gets a string from a Shared Memory Object
	int size = mem_size(smo);

	char *tmp = (char *)malloc(size);

	if(read_mem(smo, 0, size, tmp))
	{
		return NULL;		
	}

	return tmp;
}

static pipeid = 0;

unsigned short get_new_pipeid()
{
	return pipeid++; // FIXME: please do something better
}

struct pipe_buffer_block *alloc_block()
{
	struct pipe_buffer_block *b = (struct pipe_buffer_block *)malloc(sizeof(struct pipe_buffer_block));

	int i = 0;
	while(i < PIPES_BUFFER_SIZE){ b->bytes[i] = 0;i++;}

	return b;
}

void free_block(struct pipe_buffer_block *block)
{
	free(block);
}


