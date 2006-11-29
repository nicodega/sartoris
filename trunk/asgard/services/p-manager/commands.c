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

#include "pmanager_internals.h"
#include <os/layout.h>
#include <sartoris/syscall.h>

/* use static arrays, so they are not initialized every time the */
/* functions are called, and the compiler can optimize further.  */

extern struct pm_task   task_info[MAX_TSK];
extern struct pm_thread thread_info[MAX_THR];
extern int running;
extern int stdfs_task_id;

static int   tsk_lower_bound[] = { FIRST_USER_TSK, FIRST_SERV_TSK };
static int   tsk_upper_bound[] = { LAST_USER_TSK, LAST_SERV_TSK };

static int   thr_lower_bound[] = { FIRST_USER_THR, FIRST_SERV_THR };
static int   thr_upper_bound[] = { LAST_USER_THR, LAST_SERV_THR };

static int   tsk_priv[] = { /* Proc */ 2, /* Serv */ 0 };

void pm_process_command(struct pm_msg_generic *msg, int task_id) {

  struct pm_msg_response pm_res;

  switch (msg->pm_type) {
    case PM_CREATE_TASK: {
      
      pm_create_task((struct pm_msg_create_task *)msg, task_id);
      
      break;
    }
    
    case PM_CREATE_THREAD: {      
    
		pm_create_thread((struct pm_msg_create_thread*)msg, task_id);

      break;
    }
    
    case PM_DESTROY_TASK: {
      int destroy_task_id = ((struct pm_msg_destroy_task*)msg)->task_id;

      if (task_info[destroy_task_id].state != TSK_NOTHING) {

			task_info[destroy_task_id].ret_value = ((struct pm_msg_destroy_task*)msg)->ret_value;
			task_info[destroy_task_id].destroy_sender_id = task_id;
			task_info[destroy_task_id].state = TSK_KILLED;
			task_info[destroy_task_id].destroy_req_id = msg->req_id;
			task_info[destroy_task_id].destroy_ret_port = msg->response_port;

			pm_request_close( ((struct pm_msg_destroy_task*)msg)->task_id );
      }
      else
      {
			pm_inform_result((struct pm_msg_generic *) msg, task_id, PM_TASK_ID_INVALID, 0, 0);
      }

      break;
    }
    
    case PM_DESTROY_THREAD: {

	  pm_destroy_thread(((struct pm_msg_destroy_thread*)msg)->thread_id);

      pm_res.status = 0;
      pm_res.pm_type = PM_DESTROY_THREAD;
	
      send_msg(task_id, msg->response_port, &pm_res);

      break;
    }
    
    case PM_BLOCK_THREAD: {
      
      /* not implemented yet */

      break;
    }
    
    case PM_UNBLOCK_THREAD: {
      
      /* not implemented yet */

      break;
    }

    case PM_TASK_FINISHED: {

       
	if (task_info[task_id].state != TSK_NOTHING) {

	   task_info[task_id].ret_value = ((struct pm_msg_finished*)msg)->ret_value;
	   task_info[task_id].destroy_sender_id = -1;
	   task_info[task_id].state = TSK_KILLED;
	   task_info[task_id].destroy_req_id = -1;
	   task_info[task_id].destroy_ret_port = -1;

       pm_request_close( task_id );
	}

      break;
    }
  }
}

void pm_create_task(struct pm_msg_create_task *msg, int creator_task_id) 
{
  
  struct task mk_task;
  int   new_task_id = msg->new_task_id;
  short flags       = msg->flags;
  int   type        = (flags & SERVER_TASK) / SERVER_TASK;
  int   i;

  if (new_task_id > 0) {
  

    if (tsk_lower_bound[type] <=  new_task_id && 
	new_task_id       <=  tsk_upper_bound[type]) {
      
      if (task_info[new_task_id].state != TSK_NOTHING ) {
	/* new_task_id is not free to be used */
	pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_ID_TAKEN, 0, 0);
	return;
      }

    } else {
      /* new_task_id is out of range */
      
      pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_ID_INVALID, 0, 0);
      return;
    }
  } else {
    /* search for a free task id */
    
    new_task_id = pm_get_free_task_id(tsk_lower_bound[type], tsk_upper_bound[type]);
    
    if (new_task_id < 0) {
      

      pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_NO_ROOM, 0, 0);
      return;
      
    }
  }
  
  if (read_mem(msg->path_smo_id, 0, /*MAX_PATH_LENGTH*/mem_size(msg->path_smo_id), & (task_info[new_task_id].full_path)) != SUCCESS) {

    pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_SMO_ERROR, 0, 0);
    return;

  }
  
  task_info[new_task_id].page_dir = pm_get_page();

  if (task_info[new_task_id].page_dir == 0) 
  {
    pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_OUT_OF_MEM, 0, 0);
    return;
  }

  /* Set taken for page directory */
  struct taken_entry tentry;

  CREATE_TAKEN_DIR_ENTRY(&tentry, new_task_id);
  pm_assign(task_info[new_task_id].page_dir, PMAN_TBL_ENTRY_NULL, &tentry );
  
  /* if we got here, task id is valid, the task slot is free,
     the path got through fine (smo is ok) and we have enough
     memory at least for a new page directory */
  
  /* everything is fine. spawn task, don't fly upside down and be happy! */

  /* save info */
  task_info[new_task_id].state = TSK_LOADING;
  task_info[new_task_id].flags = (flags & SERVER_TASK)? TSK_FLAG_SERVICE : 0;
  task_info[new_task_id].fs_task_id = stdfs_task_id;
  task_info[new_task_id].open_fd = -1;
  task_info[new_task_id].first_thread = -1;
  task_info[new_task_id].num_threads = 0;
  task_info[new_task_id].creator_task_id = creator_task_id;
  task_info[new_task_id].response_port = msg->response_port;
  task_info[new_task_id].req_id = msg->req_id;
  task_info[new_task_id].destroy_sender_id = -1;
  task_info[new_task_id].elf_pheaders = NULL;
  task_info[new_task_id].task_smo_id = -1;
  task_info[new_task_id].swap_free_addr = SARTORIS_PROCBASE_LINEAR;
  task_info[new_task_id].swap_page_count = task_info[new_task_id].page_count = 0;
  
  /* create microkernel task */
  mk_task.mem_adr = (void*)SARTORIS_PROCBASE_LINEAR;    /* paging mapped the memory here */
  mk_task.size = PM_TASK_SIZE;       /* task size is now 4GB - kernel space size (0x800000) */
  mk_task.priv_level = tsk_priv[type];       

  create_task(new_task_id, &mk_task, NO_TASK_INIT, 0);

  pm_page_in(new_task_id, 0, task_info[new_task_id].page_dir, 0, 0);

  void *pg_address = pm_get_page();

  if(pg_address == 0) 
  {
	pm_put_page(task_info[new_task_id].page_dir);
    pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_TASK_OUT_OF_MEM, 0, 0);
    return;
  }

  CREATE_TAKEN_TBL_ENTRY(&tentry, new_task_id, PM_LINEAR_TO_DIR(SARTORIS_PROCBASE_LINEAR), (task_info[new_task_id].flags & TSK_FLAG_SERVICE)? TAKEN_EFLAG_SERVICE : 0);
  pm_assign(pg_address, PMAN_TBL_ENTRY_NULL, &tentry );

  pm_page_in(new_task_id, (void*) SARTORIS_PROCBASE_LINEAR, LINEAR2PHYSICAL(pg_address), 1, PGATT_WRITE_ENA);
  
  /* start file open */
  pm_request_file_open(new_task_id, msg);   /* FIXME */
}


void pm_create_thread(struct pm_msg_create_thread *msg, int creator_task_id ) {

  
  struct thread mk_thread;
  int new_thread_id;
  struct pm_msg_response pm_res;

  if (msg->task_id < 0 || 
      msg->task_id >= MAX_TSK || 
      task_info[msg->task_id].state != TSK_NORMAL) {
    
		pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
		return;
    
  }
  
  if (FIRST_USER_TSK <= msg->task_id &&
      msg->task_id < LAST_USER_TSK) {

    new_thread_id = pm_get_free_thread_id(FIRST_USER_THR, LAST_USER_THR);

  } else {
    
    new_thread_id = pm_get_free_thread_id(FIRST_SERV_THR, LAST_SERV_THR);

  }

  if (new_thread_id < 0) {
    
    pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_NO_ROOM, 0, 0);
    return;

  }

  /* Ok, task id is fine and we got a free thread id. Let's get through with it. */
  
  
  /* save info */
  thread_info[new_thread_id].task_id = msg->task_id;
  thread_info[new_thread_id].state = THR_WAITING;
  thread_info[new_thread_id].flags = 0;
  thread_info[new_thread_id].fault_address = 0;
  thread_info[new_thread_id].page_in_address = 0;
  thread_info[new_thread_id].fault_smo_id = -1;
  thread_info[new_thread_id].next_thread = -1; 
  thread_info[new_thread_id].page_displacement = 0;
  thread_info[new_thread_id].fault_next_thread = -1;
  thread_info[new_thread_id].interrupt = 0;
  thread_info[new_thread_id].swaptbl_next = -1;

  /* create microkernel thread */
  mk_thread.task_num = msg->task_id;
  mk_thread.invoke_mode = PRIV_LEVEL_ONLY;
  mk_thread.invoke_level = 0;
  mk_thread.ep = (void*) msg->entry_point;

  /* Find a location for the thread stack. */
  int curr_thr, stack_slot = -1;
 
  /* See if a given slot is free, if not increment and test again... */
  do
  {
	stack_slot++;
	curr_thr = task_info[msg->task_id].first_thread;

	/* See if slot is taken */
	while(curr_thr != -1 && (thread_info[curr_thr].stack_addr != (void*)STACK_ADDR(PM_THREAD_STACK_BASE - stack_slot * 0x20000)))
	{
		curr_thr = thread_info[curr_thr].next_thread;
	}
  }while(curr_thr != -1);

  mk_thread.stack = thread_info[new_thread_id].stack_addr = (void*)STACK_ADDR(PM_THREAD_STACK_BASE - stack_slot * 0x20000);

  /* Set thread entry point from elf file if 0 is specified. */
  if(msg->entry_point == 0)
  {
		mk_thread.ep = (void*)task_info[msg->task_id].elf_header.e_entry;
  }

  if(msg->interrupt != 0)
  {
	/* Only services will be allowed to handle interrupts. */
	if(!(task_info[msg->task_id].flags & TSK_FLAG_SERVICE))
	{
		thread_info[new_thread_id].state = THR_NOTHING;
		pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
		return;
	}

	/* Check interrupt is not already being handled */
	int i = 0;
	while(i < MAX_THR)
	{
		if(thread_info[i].state == THR_INTHNDL && thread_info[i].interrupt == msg->interrupt)
		{
			// fail 
			thread_info[new_thread_id].state = THR_NOTHING;
			pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
			return;
		}
		i++;
	}
  }
  
  if(create_thread(new_thread_id, &mk_thread))
  {
	thread_info[new_thread_id].state = THR_NOTHING;
	// FIXME: Send proper fail msg
	pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
	return;
  }
  
  if(msg->interrupt != 0)
  {
	/* create the interrupt handler */
	if(create_int_handler(msg->interrupt, new_thread_id, 1, 10))
	{
		destroy_thread(new_thread_id);
		// FIXME: send proper failure message
		thread_info[new_thread_id].state = THR_NOTHING;
		pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_ID_INVALID, 0, 0);
		return;
	}
	thread_info[new_thread_id].state = THR_INTHNDL;	// thread won't be scheduled
	thread_info[new_thread_id].interrupt = msg->interrupt;
  }

  if(task_info[msg->task_id].first_thread != -1)
  {
		thread_info[new_thread_id].next_thread = task_info[msg->task_id].first_thread;
  }

  task_info[msg->task_id].first_thread = new_thread_id;

  task_info[msg->task_id].num_threads++;

  pm_inform_result((struct pm_msg_generic *) msg, creator_task_id, PM_THREAD_OK, new_thread_id, msg->task_id);
}

void pm_inform_result(struct pm_msg_generic *msg, int task_id, int status, int new_id, int new_id_aux) {
  struct pm_msg_response msg_ans;
  
  msg_ans.pm_type = msg->pm_type;
  msg_ans.req_id  = msg->req_id;
  msg_ans.status  = status;
  msg_ans.new_id  = new_id;
  msg_ans.new_id_aux = new_id_aux;
  
  send_msg(task_id, msg->response_port, &msg_ans);
  
}

int pm_get_free_task_id(int lower_bound, int upper_bound) {
  
  int i;

  for (i=lower_bound; i <= upper_bound; i++) {
    if (task_info[i].state == TSK_NOTHING) {
      return i;
    }
  }
  
  return -1;

}

int pm_get_free_thread_id(int lower_bound, int upper_bound) {
  
  int i;

  for (i=lower_bound; i <= upper_bound; i++) {
    if (thread_info[i].state == THR_NOTHING) {
      return i;
    }
  }

  return -1;

}

int pm_destroy_thread(int thread_id)
{
	if (thread_info[thread_id].state == THR_NOTHING) 
	{
		return -1;
	} 
	else 
	{
		if (destroy_thread(thread_id) != SUCCESS) 
		{
			return -1;
		} 
		else 
		{
			task_info[thread_info[thread_id].task_id].num_threads--;

			if(task_info[thread_info[thread_id].task_id].first_thread == thread_id)
			{
				task_info[thread_info[thread_id].task_id].first_thread = thread_info[thread_id].next_thread;
			}
			else
			{
				// add a prev thread !!! this is awful
				int i = task_info[thread_info[thread_id].task_id].first_thread;
				while(i != -1 && thread_info[i].next_thread != thread_id)
				{
					i = thread_info[i].next_thread;
				}
				thread_info[i].next_thread = thread_info[thread_id].next_thread;
			}

			thread_info[thread_id].state       = THR_NOTHING;
			thread_info[thread_id].task_id     = -1;
			thread_info[thread_id].next_thread = -1;

			return 0;
		}
	}
}

int pm_destroy_task(int task_id) {

    int thread_id;
    
    if (task_info[task_id].state == TSK_NOTHING) {
        return -1;
    }

	thread_id = task_info[task_id].first_thread;

	while(thread_id != -1)
	{
		task_info[task_id].first_thread = thread_info[thread_id].next_thread;

		if(pm_destroy_thread(thread_id))
		{
			return -1;
		}
		thread_id = task_info[task_id].first_thread;
	}
    
    if (destroy_task(task_id) != SUCCESS) {
        return -1;
    }

    task_info[task_id].state = TSK_NOTHING;
    task_info[task_id].full_path[0] = '\0';
    task_info[task_id].creator_task_id = -1;
    task_info[task_id].open_fd = -1;
    task_info[task_id].first_thread = -1;
    task_info[task_id].num_threads = 0;
    
	if(task_info[task_id].elf_pheaders != NULL) free(task_info[task_id].elf_pheaders);

	task_info[task_id].elf_pheaders = NULL;

    pm_claim_address_space(task_info[task_id].page_dir);
    
}
