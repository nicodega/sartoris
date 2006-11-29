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


extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

int stdfs_task_id = OFSS_TASK;

void pm_request_file_open(int new_task_id, struct pm_msg_create_task *msg) 
{
	struct stdfss_open msg_open;

	/* we're done with the path SMO, pass it to filesystem now */

	pass_mem(msg->path_smo_id, stdfs_task_id);

	msg_open.command   = STDFSS_OPEN;
	msg_open.thr_id    = new_task_id;
	msg_open.file_path = msg->path_smo_id;
	msg_open.open_mode = STDFSS_FILEMODE_READ;
	msg_open.flags     = STDFSS_FILEMODE_MUSTEXIST;
	msg_open.ret_port  = FILEOPEN_PORT;

	send_msg(stdfs_task_id, STDFSS_PORT, &msg_open);
}

void pm_request_seek(int thread_id, unsigned int offset) 
{
	struct stdfss_seek msg_seek;

	msg_seek.command   = STDFSS_SEEK;
	msg_seek.thr_id    = thread_id;
	msg_seek.origin    = STDFSS_SEEK_SET;
	msg_seek.file_id   = task_info[thread_info[thread_id].task_id].open_fd;
	msg_seek.possition = offset;
	msg_seek.ret_port  = FILESEEK_PORT;

	send_msg(stdfs_task_id, STDFSS_PORT, &msg_seek);
}

void pm_request_read(int thread_id, void *pg_addr, unsigned int size) {

  struct stdfss_read msg_read;
  int smo_id;

  smo_id = share_mem(stdfs_task_id, pg_addr, size, READ_PERM|WRITE_PERM);
  
  msg_read.command  = STDFSS_READ;
  msg_read.thr_id   = thread_id;
  msg_read.file_id  = task_info[thread_info[thread_id].task_id].open_fd;
  msg_read.count    = size;
  msg_read.smo      = smo_id;
  msg_read.ret_port = FILEREAD_PORT;
  
  thread_info[thread_id].fault_smo_id = smo_id;

  send_msg(stdfs_task_id, STDFSS_PORT, &msg_read);
}

void pm_request_close(int task_id)
{
	 struct stdfss_close msg_close;

	 msg_close.command  = STDFSS_CLOSE;
	 msg_close.thr_id   = task_id;
	 msg_close.file_id  = task_info[task_id].open_fd;
	 msg_close.ret_port = FILECLOSE_PORT;
	 
	 send_msg(stdfs_task_id, STDFSS_PORT, &msg_close);
}

void pm_process_file_opens() 
{
    /* process fileopens from filesystem */
    
	while (get_msg_count(FILEOPEN_PORT) > 0) 
	{
		struct stdfss_open_res msg_open_res;
		int sender_task_id;

		if (get_msg(FILEOPEN_PORT, &msg_open_res, &sender_task_id ) == SUCCESS) 
		{

			int task_id = msg_open_res.thr_id;

			if ( !(task_info[task_id].flags & TSK_FLAG_FILEOPEN) ) 
			{
				if (msg_open_res.ret == STDFSSERR_OK) 
				{
					task_info[task_id].open_fd = msg_open_res.file_id;
					task_info[task_id].flags |= TSK_FLAG_FILEOPEN;

					/* Begin elf loading */
					elf_begin(task_id, pm_request_elf_seek, pm_request_elf_read);
				} 
				else 
				{
					struct pm_msg_response res_msg;

					res_msg.pm_type = PM_CREATE_TASK;
					res_msg.req_id  = task_info[task_id].req_id;
					res_msg.status  = PM_TASK_IO_ERROR;
					res_msg.new_id  = task_id;
					res_msg.new_id_aux = 0;

					if(task_info[task_id].creator_task_id != -1)
					{
						res_msg.status  = PM_TASK_IO_ERROR;
						send_msg(task_info[task_id].creator_task_id, task_info[task_id].response_port, &res_msg );
					}
				}
			}
		}
	} // while
}

void pm_process_seeks() 
{
	struct taken_entry tentry;

    /* process fileseeks from filesystem */

	while (get_msg_count(FILESEEK_PORT) > 0) 
	{
		struct stdfss_res msg_seek_res;
		int sender_task_id;
	    
		if (get_msg(FILESEEK_PORT, &msg_seek_res, &sender_task_id) == SUCCESS) 
		{
			int thread_id = msg_seek_res.thr_id;

			pm_request_read(thread_id, (void*)(PHYSICAL2LINEAR(thread_info[thread_id].page_in_address) + thread_info[thread_id].page_displacement), thread_info[thread_id].read_size);
		}
	}
}

void pm_process_reads() 
{
	while (get_msg_count(FILEREAD_PORT) > 0) 
	{
		struct stdfss_res msg_read_res;
		int sender_task_id;

		if (get_msg(FILEREAD_PORT, &msg_read_res, &sender_task_id) == SUCCESS) 
		{
			int thread_id = msg_read_res.thr_id;

			/* Page in */
			pm_page_in(thread_info[thread_id].task_id, 
					thread_info[thread_id].fault_address, 
					(void *)((unsigned int) thread_info[thread_id].page_in_address), 
					2, 
					thread_info[thread_id].page_perms);

			/* Un set IOLCK on the table */
			unsigned int **base = (unsigned int **)PHYSICAL2LINEAR(PG_ADDRESS(task_info[thread_info[thread_id].task_id].page_dir));
			struct taken_entry *ptentry = get_taken((void*)PG_ADDRESS(base[PM_LINEAR_TO_DIR(thread_info[thread_id].fault_address)]));

			ptentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_IOLOCK;

			claim_mem(thread_info[thread_id].fault_smo_id);

			/* Un set IOLCK on the page */
			ptentry = get_taken((void*)PG_ADDRESS(thread_info[thread_id].page_in_address));

			ptentry->data.b_pg.eflags &= ~TAKEN_EFLAG_IOLOCK;

			/* Set page taken status on our tables */
			pm_assign((void*)PG_ADDRESS(thread_info[thread_id].page_in_address), CREATE_PMAN_TBL_ENTRY(thread_info[thread_id].task_id, PM_LINEAR_TO_DIR(thread_info[thread_id].fault_address), 0 ), ptentry );

			task_info[thread_info[thread_id].task_id].page_count++;

			wake_pf_threads(thread_id);
		}
	}
}

void pm_process_closes() 
{
	/* process close responses from filesystem */
	while (get_msg_count(FILECLOSE_PORT) > 0) 
		{
			struct stdfss_res msg_close_res;
			int sender_task_id = -1;
			
			if (get_msg(FILECLOSE_PORT, &msg_close_res, &sender_task_id) == SUCCESS) 
			{
				int task_id = msg_close_res.thr_id;

				if( !(0 <= task_id && task_id < MAX_TSK) || task_info[task_id].state != TSK_KILLED) continue;	

				/* Begin swap free procedure */
				task_swap_empty(task_id);
			}
	}
}
