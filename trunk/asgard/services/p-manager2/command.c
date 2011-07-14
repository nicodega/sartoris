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
#include <proc/init_data_dl.h>
#include <services/pmanager/debug.h>

int tsk_lower_bound[] = { 1, 1 };
int tsk_upper_bound[] = { MAX_TSK, MAX_TSK };

int thr_lower_bound[] = { 1, 1 };
int thr_upper_bound[] = { MAX_THR, MAX_THR };

BOOL shutting_down = FALSE;
extern int pman_stage;

/***************************** CALLBACKS ******************************************/
INT32 cmd_task_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret);
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
	cmd_inf->command_req_id = 0;
	cmd_inf->command_ret_port = 0;
	cmd_inf->command_sender_id = 0;
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
                case PM_LOADER_READY:
                {
                    // destroy the SMO sent to the task for phdrs
                    tsk = tsk_get(task_id);

                    if(tsk != NULL)
                    {
                        loader_ready(tsk);
                    }
                    break;
                }
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
                case PM_DBG_ATTACH:
                {
                    cmd_begin_debug(task_id, (struct pm_msg_dbgattach*)&msg);
                    break;
                }
                case PM_DBG_END:
                {
                    cmd_end_debug(task_id, (struct pm_msg_dbgend*)&msg);
                    break;
                }
                case PM_DBG_TRESUME:
                {
                    cmd_resumethr_debug(task_id, (struct pm_msg_dbgtresume*)&msg);
                    break;
                }
                case PM_LOAD_LIBRARY:
                {
                    tsk = tsk_get(task_id);
                    if(!tsk) break;
                    cmd_load_library((struct pm_msg_loadlib *)&msg, task_id);
                    break;
                }
			    case PM_CREATE_TASK: 
			    {
                    tsk = tsk_get(task_id);

                    if(!tsk) break;
                    
				    cmd_create_task((struct pm_msg_create_task *)&msg, tsk);
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

                    // if it's not the creator task, ignore it
                    if(tsk->creator_task != task_id)
                    {
                        cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
                        break;
                    }

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
						
						tsk->io_finished.callback = cmd_task_fileclosed_callback;
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

                    /* Is it a request from a different process? */
                    if(destroy_task_id != task_id)
                    {
                        cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
                        break;
                    }

					tsk = tsk_get(destroy_task_id);
                    
                    struct dbg_message dbg_msg;
                    
					if(tsk && tsk->state == TSK_NORMAL)
					{                        
						if(thr_destroy_thread(((struct pm_msg_destroy_thread*)&msg)->thread_id))
                        {
                            if(tsk->flags & TSK_DEBUG)
                            {
                                dbg_msg.task = destroy_task_id;
                                dbg_msg.thread = thr->id;
                                dbg_msg.command = DEBUG_THR_DESTROYED;
                                send_msg(tsk->dbg_task, tsk->dbg_port, &dbg_msg);                                
                            }
							cmd_inform_result(&msg, task_id, PM_OK, 0, 0);
                        }
						else
							cmd_inform_result(&msg, task_id, PM_ERROR, 0, 0);
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
						
						tsk->io_finished.callback = cmd_task_fileclosed_callback;
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

	/* 
    Attempt to process commands on the queue. 
    */
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
                    if(!(tsk->flags & TSK_FLAG_SERVICE))
                        cmd_inform_result(&msg, task_id, PM_NO_PERMISSION, 0, 0);
                    else
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

				if(tsk->state == TSK_NORMAL 
                    && (tsk->flags & TSK_FLAG_SERVICE))
				{
					tsk->command_inf.callback = cmd_finished__callback;
					if(!vmm_phy_mmap(tsk, 
						(ADDR)((struct pm_msg_pmap_create*)&msg)->start_phy_addr, 
						(ADDR)(((struct pm_msg_pmap_create*)&msg)->start_phy_addr + ((struct pm_msg_pmap_create*)&msg)->length),
						(ADDR)((struct pm_msg_pmap_create*)&msg)->start_addr, 
						(ADDR)(((struct pm_msg_pmap_create*)&msg)->start_addr + ((struct pm_msg_pmap_create*)&msg)->length), 
						((struct pm_msg_pmap_create*)&msg)->flags))
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

				if(tsk->state == TSK_NORMAL 
                    && (tsk->flags & TSK_FLAG_SERVICE))
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

void cmd_inform_result(void *msg, UINT16 task_id, UINT16 status, UINT16 new_id, UINT16 new_id_aux) 
{
	struct pm_msg_response msg_ans;
  
	msg_ans.pm_type = ((struct pm_msg_generic*)msg)->pm_type;
	msg_ans.req_id  = ((struct pm_msg_generic*)msg)->req_id;
	msg_ans.status  = status;
	msg_ans.new_id  = new_id;
	msg_ans.new_id_aux = new_id_aux;
	
	send_msg(task_id, ((struct pm_msg_generic*)msg)->response_port, &msg_ans);  
}

void cmd_resumethr_debug(UINT16 dbg_task, struct pm_msg_dbgtresume *msg) 
{
	struct pm_task *tsk;
    struct pm_task *dbg;
    struct pm_thread *thr;
	
    dbg = tsk_get(dbg_task);
    thr = thr_get(msg->thread_id);

    if(thr == NULL || thr->state != THR_DBG)
    {
        if(dbg) cmd_inform_result(msg, dbg_task, PM_NOT_BLOCKED, 0, 0);
        return;
    }
    
    tsk = tsk_get(thr->task_id);

    if(tsk == NULL || tsk->state == TSK_KILLING 
        || tsk->state == TSK_KILLED 
        || dbg_task != tsk->dbg_task)
    {
        if(dbg) cmd_inform_result(msg, dbg_task, PM_INVALID_TASK, 0, 0);
        return;
    }

    thr->state = THR_WAITING;
    sch_activate(thr);
}

void cmd_begin_debug(UINT16 dbg_task, struct pm_msg_dbgattach *msg) 
{
	struct pm_task *tsk;
    struct pm_task *dbg;
	
    dbg = tsk_get(dbg_task);
    tsk = tsk_get(msg->task_id);

    if(dbg == NULL || dbg->state == TSK_KILLING || dbg->state == TSK_KILLED)
    {
        // debugger task no longer exists or is being killed
        return;
    }

    if(tsk == NULL || tsk->state == TSK_KILLING 
        || tsk->state == TSK_KILLED 
        || (dbg_task != tsk->creator_task && !(dbg->flags & TSK_FLAG_SERVICE)))
    {
        // task being debugged does not exist or is being killed, or
        // the sender is not it's creator (and it's not a service)
        cmd_inform_result(msg, dbg_task, PM_INVALID_TASK, 0, 0);
        return;
    }

    // shared lib tasks cannot be debugged
    if(!(tsk->flags & TSK_DYNAMIC))
	{
		cmd_inform_result(msg, dbg_task, PM_ERROR, 0, 0);
		return;
	}

    // is it being debugged?
    if(tsk->flags & TSK_DEBUG)
    {
        cmd_inform_result(msg, dbg_task, PM_ALREADY_DEBUGGING, 0, 0);
		return;
    }

    // start tracing all threads
    struct pm_thread *thr = tsk->first_thread;

    while(thr)
    {
        if(ttrace_begin(thr->id, dbg_task) == FAILURE)
        {
            // undo the tracings
            thr = tsk->first_thread;
            while(thr)
            {
                ttrace_end(thr->id, dbg_task);
                thr = thr->next_thread;
            }
            break;
        }
        thr = thr->next_thread;
    }

    tsk->dbg_port = msg->dbg_port;
    tsk->flags |= TSK_DEBUG;

    // add the task to the debugger dbg list
    if(dbg->dbg_first)
        dbg->dbg_first->dbg_prev = tsk;
    tsk->dbg_next = dbg->dbg_first;
    dbg->dbg_first = tsk;

    cmd_inform_result(msg, dbg_task, PM_OK, 0, 0);
}

void cmd_end_debug(UINT16 dbg_task, struct pm_msg_dbgend *msg) 
{
	struct pm_task *tsk;
    struct pm_task *dbg;
	
    tsk = tsk_get(msg->task_id);
    dbg = tsk_get(dbg_task);
        
    if(tsk == NULL || tsk->state == TSK_KILLED 
        || dbg_task != tsk->dbg_task)
    {
        // task being debugged does not exist or is killed
        if(dbg) cmd_inform_result(msg, dbg_task, PM_INVALID_TASK, 0, 0);
        return;
    }

    // shared lib tasks cannot be debugged
    if(!(tsk->flags & TSK_DYNAMIC))
	{
		if(dbg) cmd_inform_result(msg, dbg_task, PM_ERROR, 0, 0);
		return;
	}

    // is it being debugged?
    if(!(tsk->flags & TSK_DEBUG))
    {
        if(dbg) cmd_inform_result(msg, dbg_task, PM_NOT_DEBUGGING, 0, 0);
		return;
    }

    // remove the task from the dbg list
    if(tsk->dbg_prev == NULL)
    {
        dbg->dbg_first = tsk->dbg_next;
    }
    else
    {
        tsk->dbg_prev->dbg_next = tsk->dbg_next;
    }
    if(tsk->dbg_next != NULL)
        tsk->dbg_next->dbg_prev = tsk->dbg_prev;

    // stop tracing all threads
    struct pm_thread *thr = tsk->first_thread;

    while(thr)
    {
        ttrace_end(thr->id, dbg_task);
        thr = thr->next_thread;
    }

    tsk->dbg_port = -1;
    tsk->dbg_task = 0xFFFF;
    tsk->flags &= ~TSK_DEBUG;

    cmd_inform_result(msg, dbg_task, PM_OK, 0, 0);
}

void cmd_load_library(struct pm_msg_loadlib *msg, UINT16 loader_task) 
{
	struct pm_task *tsk;
	UINT32 psize;
	char *path = NULL;

	if(pman_stage == PMAN_STAGE_INITIALIZING)
	{
        pman_print_dbg("PM LOAD LIB ERR1\n");
		cmd_inform_result(msg, loader_task, PM_IS_INITIALIZING, 0, 0);
		return;
	}

    if(msg->vlow < PMAN_MAPPING_BASE || msg->vhigh < msg->vlow)
	{
        pman_print_dbg("PM LOAD LIB ERR2\n");
		cmd_inform_result(msg, loader_task, PM_INVALID_BOUNDARIES, 0, 0);
		return;
	}

    tsk = tsk_get(loader_task);

    if(tsk->flags & TSK_LOADING_LIB)
	{
		cmd_inform_result(msg, loader_task, PM_TASK_RETRY_LATER, 0, 0);
		return;
	}

    if(!(tsk->flags & TSK_DYNAMIC))
	{
		cmd_inform_result(msg, loader_task, PM_ERROR, 0, 0);
		return;
	}
    path = NULL;
	psize = mem_size(msg->path_smo_id);

    pman_print_dbg("LOADING LIB path size %i\n", psize);
	if(psize < 1)
	{
		cmd_inform_result(msg, loader_task, PM_BAD_SMO, 0, 0);
		return;
	}

	path = kmalloc(psize);

	if(path == NULL)
	{
		cmd_inform_result(msg, loader_task, PM_NOT_ENOUGH_MEM, 0, 0);
		return;
	}
	
	/* Read Path */
	if(read_mem(msg->path_smo_id, 0, psize, path) != SUCCESS) 
	{
        kfree(path);
		cmd_inform_result(msg, loader_task, PM_BAD_SMO, 0, 0);
		return;
	}
    pman_print_dbg("LOADING LIB path %s\n", path);

    tsk->command_inf.callback = cmd_finished__callback;
	tsk->command_inf.command_ret_port = msg->response_port;
	tsk->command_inf.command_req_id = msg->req_id;
	tsk->command_inf.command_sender_id = loader_task;
    tsk->flags |= TSK_LOADING_LIB;

    // load the library
    int ret = vmm_lib_load(tsk, path, psize, msg->vlow, msg->vhigh);
    if(!ret)
    {
        kfree(path);
        cmd_inform_result(msg, loader_task, PM_ERROR, 0, 0);
    }   
    else if(ret == 2)
    {
        kfree(path);
        cmd_inform_result(msg, loader_task, PM_OK, 0, 0);
        return;
    }
}

void cmd_create_task(struct pm_msg_create_task *msg, struct pm_task *ctask) 
{
	struct pm_task *tsk = NULL;
	UINT16 new_task_id = msg->new_task_id;
	UINT16 flags = msg->flags;
	UINT16 type = (flags & SERVER_TASK) / SERVER_TASK;
	UINT32 psize, ret;
	char *path = NULL;

	if(pman_stage == PMAN_STAGE_INITIALIZING)
	{
		cmd_inform_result(msg, ctask->id, PM_IS_INITIALIZING, 0, 0);
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
				cmd_inform_result(msg, ctask->id, PM_TASK_ID_TAKEN, 0, 0);
				return;
			}
	    } 
		else 
		{
			/* new_task_id is out of range */
			cmd_inform_result(msg, ctask->id, PM_TASK_ID_INVALID, 0, 0);
			return;
		}
	}
	else 
	{
		/* search for a free task id */
		new_task_id = tsk_get_id(tsk_lower_bound[type], tsk_upper_bound[type]);
    
		if(new_task_id == 0xFFFF) 
		{
			cmd_inform_result(msg, ctask->id, PM_NO_FREE_ID, 0, 0);
			return;
        }
	}
  
	tsk = tsk_create(new_task_id);
    if(tsk == NULL)
    {
        cmd_inform_result(msg, ctask->id, PM_NOT_ENOUGH_MEM, 0, 0);
        return;
    }
	path = NULL;
	psize = mem_size(msg->path_smo_id);

	if(psize < 1)
	{
        tsk_destroy(tsk);
		cmd_inform_result(msg, ctask->id, PM_BAD_SMO, 0, 0);
		return;
	}

	path = kmalloc(psize);

	if(path == NULL)
	{
        tsk_destroy(tsk);
		cmd_inform_result(msg, ctask->id, PM_NOT_ENOUGH_MEM, 0, 0);
		return;
	}
	
	/* Read Path */
	if(read_mem(msg->path_smo_id, 0, psize, path) != SUCCESS) 
	{
        tsk_destroy(tsk);
		cmd_inform_result(msg, ctask->id, PM_BAD_SMO, 0, 0);
		return;
	}

	tsk->flags = (flags & SERVER_TASK)? TSK_FLAG_SERVICE : 0;

	if(path[psize-1] == '\0') psize--;
    
    tsk->creator_task = ctask->id;
	tsk->creator_task_port = msg->response_port;
	tsk->command_inf.command_req_id = msg->req_id;

	tsk->command_inf.command_ret_port = -1;
	tsk->command_inf.command_sender_id = -1;

	ret = loader_create_task(tsk, path, psize, msg->param, type, LOADER_CTASK_TYPE_PRG);

	if(ret != PM_OK)
	{
        tsk_destroy(tsk);
		kfree(path);
		cmd_inform_result(msg, ctask->id, ret, 0, 0);
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
    
	if (task == NULL || task->state != TSK_NORMAL || (task->flags & TSK_SHARED_LIB) // task is not null, it's on normal state and it's not a shared library
        || (task->num_threads > 0 && creator_task_id != task->id)                   // only the owner task can create it's threads (other than the first one)
        || (task->num_threads == 0 && creator_task_id != task->creator_task)        // only the creator task can create the first thread
        || (msg->interrupt != 0 && creator_task_id != task->id))                    // only the owner task can create an interrupt thread
	{
		cmd_inform_result(msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
		return;
	}

    if (new_thread_id != 0xFFFF && thr_get(new_thread_id)) 
	{
		cmd_inform_result(msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
		return;
	}

	new_thread_id = thr_get_id(0, MAX_THR);
	
	if(new_thread_id == 0xFFFF) 
	{
		cmd_inform_result(msg, creator_task_id, PM_THREAD_NO_ROOM, 0, 0);
		return;
	}

	/* Ok, task id is fine and we got a free thread id. Let's get through with it. */
	thread = thr_create(new_thread_id, task);

    if(thread == NULL) 
	{
		cmd_inform_result(msg, creator_task_id, PM_NOT_ENOUGH_MEM, 0, 0);
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
    if(msg->stack_addr != NULL)
    {
        // see it it's a valid stack address
        if(task->num_threads == 1 || (UINT32)msg->stack_addr > task->vmm_info.max_addr)
        {
            cmd_inform_result(msg, creator_task_id, PM_THREAD_INVALID_STACK, 0, 0);
			return;
        }

        mk_thread.stack = thread->stack_addr = (ADDR)STACK_ADDR(msg->stack_addr);
    }
    else
    {
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
    }
	/* Set thread entry point from elf file if 0 is specified. 
       If this is a dynamic task (i.e. uses shared libraries)
       we will set the entry point to that of the ld service.
    */
    if(msg->interrupt != 0 || task->num_threads > 1)
    {
        if(msg->entry_point == 0)
		    mk_thread.ep = (ADDR)task->loader_inf.elf_header.e_entry;
    }
    else
    {
	    mk_thread.ep = loader_task_ep(task);
    }

	if(msg->interrupt != 0)
	{
		/* Only services will be allowed to handle interrupts. */
		/* Check interrupt is not already being handled */
		if(!(task->flags & TSK_FLAG_SERVICE) || !int_can_attach(thread, msg->interrupt))
		{
			thread->state = THR_KILLED;
            thr_destroy_thread(thread->id);
			cmd_inform_result(msg, creator_task_id, PM_THREAD_INT_TAKEN, 0, 0);
			return;
		}		
	}

	if(create_thread(new_thread_id, &mk_thread))
	{
		thread->state = THR_KILLED;
        thr_destroy_thread(thread->id);
		cmd_inform_result(msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
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
			cmd_inform_result(msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
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
			UINT32 *size = (UINT32*)((UINT32)pg + stackpad);

            if(task->flags & TSK_DYNAMIC)
            {
                pman_print_dbg("COMMAND: Dynamic task init creation. Thread EP: %x Task EP: %x\n", ((msg->entry_point == 0)? task->loader_inf.elf_header.e_entry : msg->entry_point), mk_thread.ep);
                struct init_data_dl *idatd = (struct init_data_dl *)((UINT32)pg + stackpad - sizeof(struct init_data_dl));

                idatd->ldexit = NULL;
                idatd->creator_task = creator_task_id;
                idatd->param = task->loader_inf.param;
			    idatd->bss_end = task->tsk_bss_end;
			    idatd->curr_limit = task->vmm_info.max_addr;
                idatd->prg_start = ((msg->entry_point == 0)? (UINT32)task->loader_inf.elf_header.e_entry : (UINT32)msg->entry_point);
                idatd->ld_dynamic = (PMAN_MAPPING_BASE + (UINT32)loader_lddynsec_addr());
                idatd->ld_start = PMAN_MAPPING_BASE;
                idatd->ld_size = ld_size;                                       // comes from loader.h
                idatd->phsmo = task->loader_inf.phdrs_smo;
                idatd->phsize = task->loader_inf.elf_header.e_phentsize;
                idatd->phcount = task->loader_inf.elf_header.e_phnum;
                *size = sizeof(struct init_data_dl);
            }
            else
            {
                struct init_data *idat = (struct init_data *)((UINT32)pg + stackpad - sizeof(struct init_data));
			    idat->ldexit = NULL;
                idat->creator_task = creator_task_id;
                idat->param = task->loader_inf.param;
			    idat->bss_end = task->tsk_bss_end;
			    idat->curr_limit = task->vmm_info.max_addr;
			    *size = sizeof(struct init_data);
            }
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
			cmd_inform_result(msg, creator_task_id, PM_THREAD_FAILED, 0, 0);
			thr_destroy_thread(thread->id);
			return;
		}		
	}
	else
	{	
        /* If it's task is being debugged, 
        init trace and let the debugger know
        a new thread was created            */
        if(task->flags & TSK_DEBUG)
        {
            struct dbg_message dbg_msg;
            dbg_msg.task = thread->task_id;
            dbg_msg.thread = thread->id;
            dbg_msg.command = DEBUG_THR_CREATED;

            if(ttrace_begin(thread->id, task->dbg_task) == FAILURE)
            {
                dbg_msg.status = DEBUG_STATUS_DBG_FAILED;
                send_msg(task->dbg_task, task->dbg_port, &dbg_msg);
            }
            else
            {
                dbg_msg.status = DEBUG_STATUS_OK;
                send_msg(task->dbg_task, task->dbg_port, &dbg_msg);
            }
        }

		/* Add thread to scheduler list */
		sch_add(thread);

		/* Begin scheduling! */
		sch_activate(thread);
	}

	cmd_inform_result(msg, creator_task_id, PM_THREAD_OK, new_thread_id, msg->task_id);
}

/* 
This function will be called by IO subsystem when a task file was closed because of 
termination.
*/
INT32 cmd_task_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret) 
{
	struct pm_task *task = NULL;

	if(ioret != IO_RET_OK) return 0;

	task = tsk_get(iosrc->id);
    
    /* 
    If there are any tasks being debugged, finish 
    the traces.
    */
    while(task->dbg_first)
    {
        if(task->dbg_first->dbg_prev)
        {
            task->dbg_first->dbg_prev->dbg_next = NULL;
            task->dbg_first->dbg_prev = NULL;
        }
        struct pm_thread *thr = task->dbg_first->first_thread;
        while(thr)
        {
            ttrace_end(thr->id, task->id);
            thr = thr->next_thread;
            if(thr->state == THR_DBG)
            {
                thr->state = THR_WAITING;
                sch_activate(thr);
            }
        }
        task->flags &= ~TSK_DEBUG;
        task->dbg_task = 0xFFFF;
        task->dbg_first = task->dbg_first->dbg_next;
    }

    /* if the task is being debugged remove it from the dbg list */
    if(task->flags & TSK_DEBUG)
    {
        if(task->dbg_prev == NULL)
        {
            struct pm_task *dtask = tsk_get(task->dbg_task);
            dtask->dbg_first = task->dbg_next;
        }
        else
        {
            task->dbg_prev->dbg_next = task->dbg_next;
        }
        if(task->dbg_next != NULL)
            task->dbg_next->dbg_prev = task->dbg_prev;
    }

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

	if(task->creator_task != 0xFFFF)
	{
		// inform the creating task 
		pm_finished.pm_type = PM_TASK_FINISHED;
		pm_finished.req_id =  0;
		pm_finished.taskid = task->id;
		pm_finished.ret_value = task->command_inf.ret_value;

		send_msg(task->creator_task, task->creator_task_port, &pm_finished);
	}    

    if(task->dbg_task != 0xFFFF)
	{
        struct dbg_message dbg_msg;
		// inform the creating task 
		dbg_msg.command = DEBUG_TASK_FINISHED;
		dbg_msg.task = task->id;
		
		send_msg(task->dbg_task, task->dbg_port, &dbg_msg);
	}

	task->command_inf.command_sender_id = 0xFFFF;
    task->creator_task = 0xFFFF;
    task->dbg_task = 0xFFFF;
	
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


