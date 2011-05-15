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

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "types.h"
#include "command.h"
#include "interrupts.h"
#include "task_thread.h"
#include <services/pmanager/services.h>
#include "kmalloc.h"
#include "vm.h"
#include "exception.h"
#include "layout.h"
#include "loader.h"
#include <proc/init_data.h>

static int tsk_lower_bound[] = { 1, 1 };
static int tsk_upper_bound[] = { MAX_TSK, MAX_TSK };

static int thr_lower_bound[] = { 1, 1 };
static int thr_upper_bound[] = { MAX_THR, MAX_THR };

BOOL shutting_down = FALSE;
extern int pman_stage;

/***************************** CALLBACKS ******************************************/
INT32 cmd_task_destroyed_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 cmd_swap_freed_callback(struct fsio_event_source *iosrc, INT32 ioret);
UINT32 cmd_fmap_callback(struct pm_task *task, INT32 ioret);
INT32 cmd_fmap_finish_callback(struct pm_task *task, INT32 ioret);
INT32 cmd_finished__callback(struct pm_task *task, INT32 ioret);
/**********************************************************************************/

void cmd_init()
{
	command_queue.first = command_queue.last = NULL;
	command_queue.total = 0;
}

void cmd_info_init(struct command_info *cmd_inf)
{
	cmd_inf->creator_task_id = 0;
	cmd_inf->command_req_id = 0;
	cmd_inf->command_ret_port = 0;
	cmd_inf->command_sender_id = 0;
	cmd_inf->req_id = 0;
	cmd_inf->response_port = 0;
	cmd_inf->ret_value = 0;
	cmd_inf->callback = NULL;
	cmd_inf->executing = NULL;
}

/* Process incoming commands */
void cmd_process_msg()
{
	struct pm_msg_generic msg;
	INT32 task_id, retval;
	struct pm_task *tsk;
	struct pm_thread *thr;
	struct fmap_params params;
	UINT16 destroy_task_id;
	struct pending_command *cmd = NULL;
    struct pm_msg_phymem_response pres;
				
	/*
	For some commands like FMAP, we should only process one command at the same time for each task. 
	*/

	/* Add commands to the queue. */
	while(get_msg_count(PMAN_COMMAND_PORT) > 0) 
	{
		if(shutting_down)
		{
			cmd_shutdown_step();
		}
		else if(get_msg(PMAN_COMMAND_PORT, &msg, &task_id) == SUCCESS) 
		{			
			switch(msg.pm_type) 
			{                
                case PM_PHYMEM: 
				{
					/* if it's a service, it's entitled to know 
                    it's physical memory base address. */
                    tsk = tsk_get(task_id);

                    if(tsk != NULL && (tsk->flags & (TSK_FLAG_SERVICE|TSK_LOW_MEM))==(TSK_FLAG_SERVICE|TSK_LOW_MEM))
                    {
					    pres.pm_type = msg.pm_type;
	                    pres.req_id  = msg.req_id;
	                    pres.physical  = (UINT32)vmm_get_physical(task_id, (ADDR)((UINT32)((struct pm_msg_phymem*)&msg)->linear + SARTORIS_PROCBASE_LINEAR));
	
	                    send_msg(task_id, msg.response_port, &pres);
                    }
					break;
				}
				case PM_SHUTDOWN: 
				{
					/* Begin shutdown */
					cmd_shutdown_step();
					break;
				}
				case PM_CREATE_TASK: 
				{
					cmd_create_task((struct pm_msg_create_task *)&msg, task_id);
					break;
				}
				case PM_CREATE_THREAD: 
				{      
					cmd_create_thread((struct pm_msg_create_thread*)&msg, task_id);
					break;
				}
				case PM_DESTROY_TASK: 
				{
					destroy_task_id = ((struct pm_msg_destroy_task*)&msg)->task_id;

					// If sender is the task itself ignore it
					if(task_id == destroy_task_id)
						break;

					/* Trying to destroy PMAN? */
					if(destroy_task_id == PMAN_TASK)
					{
						/* Suffer The concecuences!!!!!! */
						fatal_exception(task_id, PMAN_RULEZ);
						break;
					}

					tsk = tsk_get(destroy_task_id);

					if(tsk && tsk->state != TSK_KILLING && tsk->state != TSK_KILLED) 
					{
						// If task is executing a command, fail
						if(tsk->command_inf.executing != NULL)
						{
							cmd_inform_result(&msg, task_id, PM_TASK_RETRY_LATER, 0, 0);
							break;
						}

                        // deactivate task threads
                        thr = tsk->first_thread;

                        while(thr)
                        {
                            sch_deactivate(thr);
                            thr = thr->next_thread;
                        }

						/* Remove all pending commands on the queue for this task */
						cmd_queue_remove_bytask(tsk);

						tsk->command_inf.ret_value = ((struct pm_msg_destroy_task*)&msg)->ret_value;
						tsk->command_inf.command_sender_id = task_id;
						tsk->command_inf.command_req_id = msg.req_id;
						tsk->command_inf.command_ret_port = msg.response_port;
						tsk->state = TSK_KILLING;
						
						tsk->io_finished.callback = cmd_task_destroyed_callback;
						io_begin_close( &tsk->io_event_src );
					}
					else
					{
						cmd_inform_result(&msg, task_id, PM_TASK_ID_INVALID, 0, 0);
					}

					break;
				}
				case PM_DESTROY_THREAD: 
				{
					tsk = NULL;
					thr = thr_get(((struct pm_msg_destroy_thread*)&msg)->thread_id);

					destroy_task_id = thr->task_id;
					thr = NULL;

					/* Trying to destroy PMAN threads? */
					if(destroy_task_id == PMAN_TASK)
					{
						/* Suffer The concecuences!!!!!! */
						fatal_exception(task_id, PMAN_RULEZ);
						break;
					}

					tsk = tsk_get(destroy_task_id);

					if(tsk && tsk->state == TSK_NORMAL)
					{
						if(thr_destroy_thread(((struct pm_msg_destroy_thread*)&msg)->thread_id))
						{
							cmd_inform_result(&msg, task_id, PM_OK, 0, 0);
						}
						else
						{
							cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
						}
					}
                    else
                    {
                        if(!tsk)
                            cmd_inform_result(&msg, task_id, PM_TASK_ID_INVALID, 0, 0);
                        else
                            cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
                    }
					
					break;
				}
				case PM_TASK_FINISHED: 
				{
					tsk = tsk_get(task_id);

					if(tsk != NULL && tsk->state == TSK_NORMAL) 
					{
						tsk->command_inf.ret_value = ((struct pm_msg_finished*)&msg)->ret_value;
						tsk->command_inf.command_sender_id = 0;
						tsk->command_inf.command_req_id = -1;
						tsk->command_inf.command_ret_port = -1;

                        // deactivate task threads
                        thr = tsk->first_thread;

                        while(thr)
                        {
                            sch_deactivate(thr);
                            thr = thr->next_thread;
                        }

                        cmd_queue_remove_bytask(tsk);

						tsk->state = TSK_KILLING;
						
						tsk->io_finished.callback = cmd_task_destroyed_callback;
						io_begin_close( &tsk->io_event_src );
					}
                    else
                    {
                        if(!tsk)
                            cmd_inform_result(&msg, task_id, PM_TASK_ID_INVALID, 0, 0);
                        else
                            cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
                    }
					break;
				}
				default:
				{
					tsk = tsk_get(task_id);

					if(tsk != NULL && tsk->state == TSK_NORMAL)
					{
						if(!cmd_queue_add(&msg, (UINT16)task_id))
						{
							cmd_inform_result(&msg, task_id, PM_NOT_ENOUGH_MEM, 0, 0);
						}
					}
					else
					{
						cmd_inform_result(&msg, task_id, PM_TASK_ID_INVALID, 0, 0);	
					}					
				}
			}
		}
	}

	/* Attempt to process commands on the queue. */
	cmd = command_queue.first;

	while(cmd != NULL) 
	{
		pman_print_and_stop("COMMAND.c: Got a queue COMMAND! STOP ");

		msg = cmd->msg;
		task_id = (cmd->sender_task & 0x0000FFFF);

		switch(msg.pm_type) 
		{
			/* File mapping functions */
			case PM_FMAP: 
			{
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;

				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					if(read_mem(((struct pm_msg_fmap*)&msg)->params_smo, 0, sizeof(params), &params) != SUCCESS)
					{
						cmd_inform_result(&msg, task_id, PM_BAD_SMO, 0, 0);
						continue;
					}

					tsk->command_inf.callback = cmd_finished__callback;
					tsk->command_inf.command_ret_port = msg.response_port;
					tsk->command_inf.command_req_id = msg.req_id;
					tsk->command_inf.command_sender_id = task_id;

					retval = vmm_fmap(task_id, params.fileid, params.fs_service, (ADDR)params.addr_start, params.length, params.perms, params.offset);
					
					if(retval != PM_OK)
						cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);					
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}

				break;
			}
			case PM_FMAP_FINISH: 
			{
				/* Close MMAP */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					vmm_fmap_release(tsk, ((struct pm_msg_fmap_finish*)&msg)->address);
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				
				break;
			}
			case PM_FMAP_FLUSH: 
			{
				/* Flush FMMAP */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					vmm_fmap_flush(tsk, ((struct pm_msg_fmap_finish*)&msg)->address);
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				
				break;
			}
			/* Physical mapping functions */
			case PM_PMAP_CREATE: 
			{
				/* Create PMMAP */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					if(!vmm_phy_mmap(tsk, 
						(ADDR)((struct pm_msg_pmap_create*)&msg)->start_phy_addr, 
						(ADDR)(((struct pm_msg_pmap_create*)&msg)->start_phy_addr + ((struct pm_msg_pmap_create*)&msg)->length),
						(ADDR)((struct pm_msg_pmap_create*)&msg)->start_addr, 
						(ADDR)(((struct pm_msg_pmap_create*)&msg)->start_addr + ((struct pm_msg_pmap_create*)&msg)->length), 
						(((struct pm_msg_pmap_create*)&msg)->perms & PM_PMAP_EXCLUSIVE)))
					{
						cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
					}
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				
				break;
			}
			case PM_PMAP_REMOVE: 
			{
				/* Close MMAP */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					vmm_phy_umap(tsk, (ADDR)((struct pm_msg_pmap_remove*)&msg)->start_addr);					
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				
				break;
			}
			/* Memory sharing functions */
			case PM_SHARE_MEM: 
			{
				/* Create a shared memory region */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					if(!vmm_share_create(tsk, (ADDR)((struct pm_msg_share_mem*)&msg)->start_addr, ((struct pm_msg_share_mem*)&msg)->length, ((struct pm_msg_share_mem*)&msg)->perms))
						cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);					
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				break;
			}
			case PM_UNSHARE_MEM: 
			case PM_MMAP_REMOVE:
			{
				/* Close MMAP */
				tsk = tsk_get(task_id);

                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
                    vmm_share_remove(tsk, ((struct pm_msg_share_mem_remove*)&msg)->start_addr);					
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				
				break;
			}
			/* Memory sharing mapping functions */
			case PM_MMAP_CREATE: 
			{
				/* Create a shared memory region */
				tsk = tsk_get(task_id);
                
                if(!tsk) continue;

				if(tsk->command_inf.executing != NULL)
					break;
				cmd_queue_remove(cmd);
				tsk->command_inf.executing = cmd;

				if(tsk->state == TSK_NORMAL)
				{
					tsk->command_inf.callback = cmd_finished__callback;
					if(!vmm_share_map(((struct pm_msg_mmap*)&msg)->shared_mem_id, tsk, (ADDR)((struct pm_msg_mmap*)&msg)->start_addr, ((struct pm_msg_mmap*)&msg)->length, ((struct pm_msg_mmap*)&msg)->perms))
						cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);					
				}
				else
				{
					cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
				}
				break;
			}
		}
	}
}

void cmd_inform_result(struct pm_msg_generic *msg, UINT16 task_id, UINT16 status, UINT16 new_id, UINT16 new_id_aux) 
{
	struct pm_msg_response msg_ans;
  
	msg_ans.pm_type = msg->pm_type;
	msg_ans.req_id  = msg->req_id;
	msg_ans.status  = status;
	msg_ans.new_id  = new_id;
	msg_ans.new_id_aux = new_id_aux;
	
	send_msg(task_id, msg->response_port, &msg_ans);  
}

void cmd_create_task(struct pm_msg_create_task *msg, UINT16 creator_task_id) 
{
	struct pm_task *tsk = NULL;
	UINT16 new_task_id = msg->new_task_id;
	UINT16 flags = msg->flags;
	UINT16 type = (flags & SERVER_TASK) / SERVER_TASK;
	UINT32 psize, ret;
	char *path = NULL;

	if(pman_stage == PMAN_STAGE_INITIALIZING)
	{
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_INITIALIZING, 0, 0);
			return;
	}

	if(new_task_id != 0xFFFF) 
	{
		if (tsk_lower_bound[type] <=  new_task_id && new_task_id <=  tsk_upper_bound[type]) 
		{
			tsk = tsk_get(new_task_id);

			if(tsk != NULL) 
			{
				/* new_task_id is not free to be used */
				cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_ID_TAKEN, 0, 0);
				return;
			}
	    } 
		else 
		{
			/* new_task_id is out of range */
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_ID_INVALID, 0, 0);
			return;
		}
	}
	else 
	{
		/* search for a free task id */
		new_task_id = tsk_get_id(tsk_lower_bound[type], tsk_upper_bound[type]);
    
		if(new_task_id == 0xFFFF) 
		{
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_NO_FREE_ID, 0, 0);
			return;
        }
	}
  
	tsk = tsk_create(new_task_id);
    if(tsk == NULL)
    {
        cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_NOT_ENOUGH_MEM, 0, 0);
        return;
    }
	path = NULL;
	psize = mem_size(msg->path_smo_id);

	if(psize < 1)
	{
        tsk_destroy(tsk);
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_BAD_SMO, 0, 0);
		return;
	}

	path = kmalloc(psize);

	if(path == NULL)
	{
        tsk_destroy(tsk);
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_NOT_ENOUGH_MEM, 0, 0);
		return;
	}
	
	/* Read Path */
	if(read_mem(msg->path_smo_id, 0, psize, path) != SUCCESS) 
	{
        tsk_destroy(tsk);
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_BAD_SMO, 0, 0);
		return;
	}

	tsk->flags = (flags & SERVER_TASK)? TSK_FLAG_SERVICE : 0;

	if(path[psize-1] == '\0') psize--;
    
	tsk->command_inf.creator_task_id = creator_task_id;
	tsk->command_inf.req_id = msg->req_id;
	tsk->command_inf.response_port = msg->response_port;
	tsk->command_inf.command_sender_id = 0;
	tsk->command_inf.command_ret_port = -1;
	tsk->command_inf.command_req_id = -1;

	ret = loader_create_task(tsk, path, psize, msg->param, type, FALSE);

	if(ret != PM_OK)
	{
        tsk_destroy(tsk);
		kfree(path);
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, ret, 0, 0);
	}	
}

void cmd_create_thread(struct pm_msg_create_thread *msg, UINT16 creator_task_id) 
{
	struct thread mk_thread;
	UINT16 new_thread_id;
	struct pm_thread *curr_thr;
	INT32 stack_slot = -1;
	struct pm_task *task = tsk_get(msg->task_id);
	struct pm_thread *thread = NULL; 
	ADDR pg = NULL;

	if (task == NULL || task->state != TSK_NORMAL) 
	{
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
		return;
	}

	new_thread_id = thr_get_id(0, MAX_THR);
	
	if(new_thread_id == 0xFFFF) 
	{
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_NO_ROOM, 0, 0);
		return;
	}

	/* Ok, task id is fine and we got a free thread id. Let's get through with it. */
	thread = thr_create(new_thread_id, task);

    if(thread == NULL) 
	{
		cmd_inform_result((struct pm_msg_generic *)msg, creator_task_id, PM_NOT_ENOUGH_MEM, 0, 0);
		return;
	}

	/* save info */
	thread->state = THR_WAITING;
    	
	/* Create microkernel thread */
	mk_thread.task_num = msg->task_id;
	mk_thread.invoke_mode = PRIV_LEVEL_ONLY;
	mk_thread.invoke_level = 0;
	mk_thread.ep = (ADDR)msg->entry_point;

	thread->io_event_src.file_id = task->io_event_src.file_id;
	thread->io_event_src.fs_service = task->io_event_src.fs_service;
	thread->io_event_src.size = task->io_event_src.size;

	/* Find a location for the thread stack. */

	/* See if a given slot is free, if not increment and test again... */
	do
	{
		stack_slot++;

		/* See if slot is taken */
		curr_thr = task->first_thread;
		while(curr_thr != NULL && (curr_thr->stack_addr != (ADDR)STACK_ADDR(PMAN_THREAD_STACK_BASE - stack_slot * 0x20000)))
		{
			curr_thr = curr_thr->next_thread;
		}
	}while(curr_thr != NULL);

	mk_thread.stack = thread->stack_addr = (ADDR)STACK_ADDR(PMAN_THREAD_STACK_BASE - stack_slot * 0x20000);
        
	/* Set thread entry point from elf file if 0 is specified. */
	if(msg->entry_point == 0)
		mk_thread.ep = (ADDR)task->loader_inf.elf_header.e_entry;

	if(msg->interrupt != 0)
	{
		/* Only services will be allowed to handle interrupts. */
		/* Check interrupt is not already being handled */
		if(!(task->flags & TSK_FLAG_SERVICE) || !int_can_attach(thread, msg->interrupt))
		{
			thread->state = THR_KILLED;
            thr_destroy_thread(thread->id);
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_INT_TAKEN, 0, 0);
			return;
		}		
	}

	if(create_thread(new_thread_id, &mk_thread))
	{
		thread->state = THR_KILLED;
        thr_destroy_thread(thread->id);
		cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
		return;
	}
	/* 
		- Get a page for the stack if it's the first thread
		  so we can put init info for the task in there.
	    - Get a page for the stack if it's an interrupt handler
	*/
	if((task->num_threads == 1 && !(task->flags & TSK_FLAG_SYS_SERVICE)) || msg->interrupt != 0)	
	{
		/* Lets see if the page table is present on the page directory and if not give it one */
		if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(((UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR))].ia32entry.present == 0)
		{
			/* Get a Page and set taken */
			pg = vmm_get_tblpage(task->id, PG_ADDRESS(((UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR)));
			
			/* Page in the table on task linear space. */
			pm_page_in(task->id, (ADDR)PG_ADDRESS(((UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR)), (ADDR)LINEAR2PHYSICAL(pg), 1, PGATT_WRITE_ENA);

			task->vmm_info.page_count++;
		}

		pg = vmm_get_page(thread->task_id, (UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR);

		if(pg == NULL)
		{
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
			thr_destroy_thread(thread->id);
			return;
		}

		/* If it's a page of a system service, lock the page */
		if(task->flags & TSK_FLAG_SYS_SERVICE) 
			vmm_set_flags(thread->task_id, pg, TRUE, TAKEN_EFLAG_SERVICE, TRUE);
        
        pm_page_in(thread->task_id, (ADDR)((UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR), (ADDR)LINEAR2PHYSICAL(pg), 2, PGATT_WRITE_ENA);
		
        // set init data at the begining of the stack
		if(pg != NULL && msg->interrupt == 0)
		{			
			UINT32 stackpad = (UINT32)thread->stack_addr % 0x1000;
			struct init_data *idat = (struct init_data *)((UINT32)pg + stackpad - sizeof(struct init_data));
			UINT32 *size = (UINT32*)((UINT32)pg + stackpad);

            idat->creator_task = creator_task_id;
            idat->param = task->loader_inf.param;
			idat->bss_end = task->tsk_bss_end;
			idat->curr_limit = task->vmm_info.max_addr;
			*size = sizeof(struct init_data);			
		}
		
		/* Page out from pman address space */
		vmm_unmap_page(thread->task_id, (UINT32)thread->stack_addr + SARTORIS_PROCBASE_LINEAR);

		task->vmm_info.page_count++;
	}
    	
	/* 
	Protocol for PMAN thread creation does not support privileges,
	so we assign the lowest value by default. 
	*/
	thread->sch.priority = 0;
	
	if(msg->interrupt != 0)
	{
		thread->state = THR_INTHNDL;	// thread won't be scheduled
		thread->interrupt = msg->interrupt;
		
		if(!int_attach(thread, msg->interrupt, (0x000000FF & msg->int_priority)))
		{
			cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
			thr_destroy_thread(thread->id);
			return;
		}		
	}
	else
	{			
		/* Add thread to scheduler list */
		sch_add(thread);

		/* Begin scheduling! */
		sch_activate(thread);
	}

	cmd_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_OK, new_thread_id, msg->task_id);
}

/* 
This function will be called by IO subsystem when a task file was closed because of 
termination.
*/
INT32 cmd_task_destroyed_callback(struct fsio_event_source *iosrc, INT32 ioret) 
{
	struct pm_task *task = NULL;

	if(ioret != IO_RET_OK) return 0;

	task = tsk_get(iosrc->id);
	
	task->io_finished.callback = cmd_swap_freed_callback;
	task->vmm_info.swap_free_addr = (ADDR)SARTORIS_PROCBASE_LINEAR;

	vmm_close_task(task);
	
	return 1;
}

/* Invoked when swap has been freed for the task */
INT32 cmd_swap_freed_callback(struct fsio_event_source *iosrc, INT32 ioret) 
{
	struct pm_msg_finished pm_finished;
	struct pm_task *task = tsk_get(iosrc->id);
    
	// if there is a destroy sender, send an ok to the task
	if( task->command_inf.command_sender_id != 0)
	{
		struct pm_msg_response msg_ans;
			
		msg_ans.pm_type = PM_DESTROY_TASK;
		msg_ans.req_id  = task->command_inf.command_req_id;
		msg_ans.status  = PM_OK;
		msg_ans.new_id  = task->id;
		msg_ans.new_id_aux = -1;
			
		send_msg(task->command_inf.command_sender_id, task->command_inf.command_ret_port, &msg_ans);
	}

	if(task->command_inf.creator_task_id != 0xFFFF)
	{
		// inform the creating task 
		pm_finished.pm_type = PM_TASK_FINISHED;
		pm_finished.req_id =  task->command_inf.command_req_id;
		pm_finished.taskid = task->id;
		pm_finished.ret_value = task->command_inf.ret_value;

		send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &pm_finished);
	}    

	task->command_inf.creator_task_id = 0xFFFF;
	
	if(shuttingDown())
		shutdown_tsk_unloaded(task->id);
	
    if(!tsk_destroy(task))
		pman_print("PMAN: Could not destroy task");

	return 1;
}

/*
Invoked when a memory mapping operation has been completed.
*/
INT32 cmd_finished__callback(struct pm_task *task, INT32 ioret) 
{
	struct pm_msg_response msg_ans;
	
	task->command_inf.callback = NULL;

	msg_ans.pm_type = task->command_inf.executing->msg.pm_type;
	msg_ans.req_id  = task->command_inf.command_req_id;
	msg_ans.status  = PM_OK;
	msg_ans.new_id  = task->id;
	msg_ans.new_id_aux = -1;

	if(task->command_inf.executing->msg.pm_type == PM_SHARE_MEM)
		msg_ans.new_id = task->command_inf.ret_value;
	

	if(ioret == IO_RET_OK)
	{
		send_msg(task->id, task->command_inf.command_ret_port, &msg_ans);	
	}
	else
	{
		msg_ans.status  = PM_IO_ERROR;
		send_msg(task->id, task->command_inf.command_ret_port, &msg_ans);	
	}
	
	kfree(task->command_inf.executing);
	task->command_inf.executing = NULL;

	return 1;
}


