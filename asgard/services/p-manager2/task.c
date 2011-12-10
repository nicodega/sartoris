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
#include "layout.h"
#include "signals.h"

static struct pm_task *task_info[MAX_TSK];
static struct pm_task pman_task;

static int tsk_priv[] = { /* Proc */ 2, /* Serv */ 0 };

void tsk_init()
{
	UINT32 i;

	for(i=0; i<MAX_TSK; i++) 
	{
        task_info[i] = NULL;
	}
}

struct pm_task *tsk_create(UINT16 id)
{
    if(task_info[id]) return NULL;

    struct pm_task *t;
    
    if(id != PMAN_TASK)
        t = kmalloc(sizeof(struct pm_task));
    else
        t = &pman_task;

    if(!t) return NULL;

    task_info[id] = t;

    t->id = id;
	t->state = TSK_NOTHING;
	t->flags = TSK_FLAG_NONE;
	t->first_thread = NULL;
	t->num_threads = 0;
    t->dbg_first = NULL;
    t->dbg_next = t->dbg_prev = NULL;
    t->dbg_task = 0xFFFF;

    int i = 0;

    for(i = 0; i < 32; i++)
        t->port_blocks[i] = 0;

	// Exceptions send port //
	t->exeptions.exceptions_port = 0xFFFF;
    t->exeptions.ecount = 0;
    t->exeptions.last_exception_addr = NULL;

	// Init Command Info //
	cmd_info_init(&t->command_inf);

	// Init loader info //
	loader_info_init(&t->loader_inf);

	// Init vmm info //
	vmm_init_task_info(&t->vmm_info);

    // init signals info //
    init_tsk_signals(t);

	// Init IO info //
	io_init_source(&t->io_event_src, FILE_IO_TASK, id);
	io_init_event(&t->io_finished, &t->io_event_src);
	t->swp_io_finished.callback = NULL;
    
	t->killed_threads = 0;

    return t;
}

// Gets the specified task
struct pm_task *tsk_get(UINT16 id)
{
	if(id >= MAX_TSK || task_info[id] == NULL) return NULL;
	return task_info[id];
}

UINT16 tsk_get_id(UINT32 lower_bound, UINT32 upper_bound) 
{  
	UINT16 i;

	for(i=lower_bound; i <= upper_bound; i++) 
	{
		if (task_info[i] == NULL || task_info[i]->state == TSK_NOTHING) 
			return i;
	}
  
	return 0xFFFF;
}


BOOL tsk_destroy(struct pm_task *task)
{
	struct pm_thread *thread = NULL;
	int ret = 0, thr_killed = 0, thr_ret = 0;
    
    if(!task) 
		return FALSE;

    if(task->state == TSK_NOTHING)
    {
        task_info[task->id] = NULL;
        kfree(task);
        return TRUE;
    }

	if(task->state == TSK_KILLED)
	{
        if(task->killed_threads != 0) return FALSE;

		vmm_claim(task);

        if(task->loader_inf.full_path != NULL) kfree(task->loader_inf.full_path);
	    if(task->loader_inf.elf_pheaders != NULL) kfree(task->loader_inf.elf_pheaders);
		
        task_info[task->id] = NULL;
		kfree(task);
		return TRUE;
	}
    
    thread = task->first_thread;

	while(thread != NULL)
	{
		task->first_thread = thread->next_thread;

		thr_ret = thr_destroy_thread(thread->id);
        ret |= thr_ret;
        if(thr_ret)
            thr_killed++;
        thread = task->first_thread;
	}
    
    if(ret == -1 || ((task->flags & TSK_SHARED_LIB) && task->vmm_info.wait_root.root != NULL)) 
    {
        if((task->flags & TSK_SHARED_LIB))
        {
            pman_print_dbg("LIB TASK NOT DESTROYED\n");
        }
        if(destroy_task(task->id) != SUCCESS)
        {
            pman_print_dbg("Destroy task failed %i ", task->id);
	        return FALSE;
        }

        task->state = TSK_KILLED;
		task->killed_threads = ret;
        return FALSE;
    }
	
	vmm_claim(task);

    if(destroy_task(task->id) != SUCCESS)
    {
        pman_print_dbg("Destroy task failed %i ", task->id);
	    return FALSE;
    }

    if(task->loader_inf.full_path != NULL) kfree(task->loader_inf.full_path);
	if(task->loader_inf.elf_pheaders != NULL) kfree(task->loader_inf.elf_pheaders);
	    
    task_info[task->id] = NULL;
	kfree(task);

	return TRUE;
}
