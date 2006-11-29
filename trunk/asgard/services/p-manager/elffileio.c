/*
*	Process and Memory Manager Service
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

/* TODO: make this function return FAILURE when message delivery fails 
         (i.e., if the fs message queue is full) */

#include <services/stds/stdfss.h>
#include <os/layout.h>
#include "pmanager_internals.h"
#include <services/pmanager/services.h>

extern int stdfs_task_id;
extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

int pm_request_elf_seek(int task_id, unsigned int offset) 
{
	struct stdfss_seek msg_seek;

	msg_seek.command   = STDFSS_SEEK;
	msg_seek.thr_id    = task_id;
	msg_seek.origin    = STDFSS_SEEK_SET;
	msg_seek.file_id   = task_info[task_id].open_fd;
	msg_seek.possition = offset;
	msg_seek.ret_port  = ELF_RESPONSE_PORT;

	send_msg(stdfs_task_id, STDFSS_PORT, &msg_seek);

	return 0;
}

int pm_request_elf_read(int task_id, int size, char *buffer)
{
	struct stdfss_read msg_read;
	int smo_id;

	smo_id = share_mem(stdfs_task_id, buffer, size, READ_PERM|WRITE_PERM);

	msg_read.command  = STDFSS_READ;
	msg_read.thr_id   = task_id;
	msg_read.file_id  = task_info[task_id].open_fd;
	msg_read.count    = size;
	msg_read.smo      = smo_id;
	msg_read.ret_port = ELF_RESPONSE_PORT;

	task_info[task_id].task_smo_id = smo_id;

	send_msg(stdfs_task_id, STDFSS_PORT, &msg_read);

	return 0;
}


void pm_process_elf_res() 
{
	struct pm_msg_response res_msg;

	res_msg.pm_type = PM_CREATE_TASK;
	res_msg.status  = PM_TASK_IO_ERROR;
	res_msg.new_id_aux = 0;

    /* process elf read/seek responses from filesistem */

    while (get_msg_count(ELF_RESPONSE_PORT) > 0) 
	{
		struct stdfss_res res;
		int sender_task_id;

        if (get_msg(ELF_RESPONSE_PORT, &res, &sender_task_id) == SUCCESS) 
		{
			int task = res.thr_id;

			res_msg.req_id  = task_info[task].req_id;
			res_msg.new_id  = task;

			if(res.command == STDFSS_READ)
			{
				if(task_info[task].flags & TSK_FLAG_ELF_HDR) /* Elf header was beaing retrieved */
				{
				
					claim_mem(task_info[task].task_smo_id);
					task_info[task].task_smo_id = -1;

					if(res.ret != STDFSSERR_OK && task_info[task].creator_task_id != -1)
					{
						res_msg.status  = PM_TASK_IO_ERROR;
						send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
						
					}
					else
					{
						if(!elf_check_header(task))
						{
							res_msg.status  = PM_TASK_INVALID_FILEFORMAT;
							send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
							task_info[task].destroy_sender_id = -1;
							task_info[task].creator_task_id = -1;
							pm_request_close(task);
							
						}
						else
						{
							task_info[task].flags &= ~TSK_FLAG_ELF_HDR;
							elf_seekphdrs(task, pm_request_elf_seek, pm_request_elf_read);
						}
					}
				}
				else if(task_info[task].flags & TSK_FLAG_ELF_PHDR) /* elf program headers where being read */
				{
					if(res.ret != STDFSSERR_OK && task_info[task].creator_task_id != -1)
					{
						res_msg.status  = PM_TASK_IO_ERROR;
						send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
					}
					else if(task_info[task].creator_task_id != -1)
					{
						if(elf_check(task))
						{
							// finished loading
							task_info[task].state = TSK_NORMAL;
							task_info[task].flags &= ~TSK_FLAG_ELF_PHDR;

							res_msg.status  = PM_TASK_OK;
							send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
						}
						else
						{
							res_msg.status  = PM_TASK_INVALID_FILEFORMAT;
							send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
							task_info[task].destroy_sender_id = -1;
							task_info[task].creator_task_id = -1;
							pm_request_close(task);
						}
					}
				}
			}
			else if (res.command == STDFSS_SEEK && (task_info[task].flags & TSK_FLAG_ELF_PHDR))
			{
				/* A seek for program headers is being performed */
				if(res.ret != STDFSSERR_OK && task_info[task].creator_task_id != -1)
				{
					res_msg.status  = PM_TASK_IO_ERROR;
					send_msg(task_info[task].creator_task_id, task_info[task].response_port, &res_msg );
				}
				else
				{
					elf_readphdrs(task, pm_request_elf_seek, pm_request_elf_read);
				}
			}
		}
    }
}

