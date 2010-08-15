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
#include "task_thread.h"
#include "command.h"
#include "loader.h"
#include "vm.h"
#include "kmalloc.h"

static struct pm_task task_info[MAX_TSK];

static int tsk_priv[] = { /* Proc */ 2, /* Serv */ 0 };

void tsk_init()
{
	UINT32 i;

	for(i=0; i<MAX_TSK; i++) 
	{
		task_info[i].id = i;
		task_info[i].state = TSK_NOTHING;
		task_info[i].flags = TSK_FLAG_NONE;
		task_info[i].first_thread = NULL;
		task_info[i].num_threads = 0;

		/* Exceptions send port */
		task_info[i].exeptions.exceptions_port = 0xFFFF;

		/* Init Command Info */
		cmd_info_init(&task_info[i].command_inf);
				
		/* Init loader info */
		loader_info_init(&task_info[i].loader_inf);

		/* Init vmm info */
		vmm_init_task_info(&task_info[i].vmm_inf);
		
		/* Init IO info */
		io_init_source(&task_info[i].io_event_src, FILE_IO_TASK, i);
		io_init_event(&task_info[i].io_finished, &task_info[i].io_event_src);
		task_info[i].swp_io_finished.callback = NULL;

		task_info[i].vmm_inf.regions.first = NULL;
		task_info[i].vmm_inf.regions.total = 0;
		task_info[i].killed_threads = 0;
	}
}

// Gets the specified task
struct pm_task *tsk_get(UINT16 id)
{
	if(id >= MAX_TSK) return NULL;
	return &task_info[id];
}

UINT16 tsk_get_id(UINT32 lower_bound, UINT32 upper_bound) 
{  
	UINT16 i;

	for(i=lower_bound; i <= upper_bound; i++) 
	{
		if (task_info[i].state == TSK_NOTHING) 
			return i;		
	}
  
	return 0xFFFF;
}


BOOL tsk_destroy(struct pm_task *task)
{
	struct pm_thread *thread = NULL;
	int ret = 0;
    
    if(task->state == TSK_NOTHING) 
		return FALSE;

	if(task->state == TSK_KILLED)
	{
		task->first_thread = NULL;
		task->num_threads = 0;

		io_init_source(&task->io_event_src, FILE_IO_TASK, task->id);
		
		/* We cannot claim memory if we are waiting for a swap read/write */
		vmm_claim(task->id);

		task->state = TSK_NOTHING;
		return TRUE;
	}
    
	thread = task->first_thread;

	while(thread != NULL)
	{
		task->first_thread = thread->next_thread;

		ret = thr_destroy_thread(thread->id);
		if(ret == -1) return FALSE;
		thread = task->first_thread;
	}
    
    if(destroy_task(task->id) != SUCCESS) 
	    return FALSE;
    
    kfree(task->loader_inf.full_path);
	task->loader_inf.full_path = NULL;
	kfree(task->loader_inf.elf_pheaders);
	task->loader_inf.elf_pheaders = NULL;

	if(ret == 0)
	{
		task->first_thread = NULL;
		task->num_threads = 0;
		io_init_source(&task->io_event_src, FILE_IO_TASK, task->id);

		/* We cannot claim memory if we are waiting for a swap read/write */
		vmm_claim(task->id);

		task->state = TSK_NOTHING;	
	}
	else
	{
		task->state = TSK_KILLED;
		task->killed_threads = ret;
	}

	return TRUE;
}
