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
#include "elf_loader.h"
#include "io.h"
#include "kmalloc.h"
#include <services/pmanager/services.h>

INT32 elf_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret);

INT32 elf_read_finished_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_msg_response res_msg;
	struct pm_task *task = NULL;

    task = tsk_get(iosrc->id);
	
    res_msg.pm_type = (task->flags & TSK_SHARED_LIB)? PM_LOAD_LIBRARY : PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;

	/* Reading has just finished for the ELF header. */
	if(ioret != IO_RET_OK)
	{
        if(task->flags & TSK_SHARED_LIB)
        {
            // we where loading a shared lib..
            vmm_lib_loaded(task, FALSE);
        }
        else
		{
            if(task->creator_task != 0xFFFF)
			    send_msg(task->creator_task, task->creator_task_port, &res_msg );
            
            task->io_finished.callback = elf_fileclosed_callback;
            io_begin_close(iosrc);
        }
	}
	else
	{
		if(!elf_check_header(task))
		{
            if(task->flags & TSK_SHARED_LIB)
            {
                // we where loading a shared lib..
                vmm_lib_loaded(task, FALSE);
            }
            else
            {
			    res_msg.status  = PM_INVALID_FILEFORMAT;
			    if(task->creator_task != 0xFFFF)
                    send_msg(task->creator_task, task->creator_task_port, &res_msg );
			
                task->io_finished.callback = elf_fileclosed_callback;

			    /* Close the file */
			    io_begin_close(iosrc);
            }
		}
		else if(elf_seekphdrs(task, io_begin_read, io_begin_seek) == -1)
		{
            if(task->flags & TSK_SHARED_LIB)
            {
                // we where loading a shared lib..
                vmm_lib_loaded(task, FALSE);
            }
            else
            {
                res_msg.status  = PM_NOT_ENOUGH_MEM;
                if(task->creator_task != 0xFFFF)
			        send_msg(task->creator_task, task->creator_task_port, &res_msg );
			
                task->io_finished.callback = elf_fileclosed_callback;

			    /* Close the file */
			    io_begin_close(iosrc);
            }
        }
	}
	return 1;
}

INT32 elf_readh_finished_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);
	struct pm_msg_response res_msg;

	res_msg.pm_type = (task->flags & TSK_SHARED_LIB)? PM_LOAD_LIBRARY : PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;

	if(ioret != IO_RET_OK)
	{
        if(task->flags & TSK_SHARED_LIB)
        {
            // we where loading a shared lib..
            vmm_lib_loaded(task, FALSE);
        }
        else
        {
		    if(task->creator_task != 0xFFFF)
		        send_msg(task->creator_task, task->creator_task_port, &res_msg );
            task->io_finished.callback = elf_fileclosed_callback;
		    io_begin_close(iosrc);
        }
	}
	else
	{        
        if(elf_check(task))
		{
            if(task->flags & TSK_SHARED_LIB)
            {
                // we where loading a shared lib..
                vmm_lib_loaded(task, TRUE);
            }
            else
            {
			    /* Does this executable contain a PT_INTERP section? */
                int phnum = task->loader_inf.elf_header.e_phnum;
                struct Elf32_Phdr *phdr;
                int i;

                for(i = 0; i < phnum; i++)
                {
                    phdr = (struct Elf32_Phdr*)(task->loader_inf.elf_pheaders + task->loader_inf.elf_header.e_phentsize * i);
                    if(phdr->p_type == PT_INTERP)
                        break;
                }

                /*
                FIXME: We should get the name of the interpreter here..
                */
                if(!loader_task_loaded(task, (phdr->p_type == PT_INTERP)? "ld.so": NULL))
                {
                    if(task->flags & TSK_SHARED_LIB)
                    {
                        // we where loading a shared lib..
                        vmm_lib_loaded(task, FALSE);
                    }
                    else
                    {
                        task->io_finished.callback = elf_fileclosed_callback;
                        io_begin_close(iosrc);
                    }
                }
            }
		}
		else
		{
            if(task->flags & TSK_SHARED_LIB)
            {
                // we where loading a shared lib..
                vmm_lib_loaded(task, FALSE);
            }
            else
			{
                if(task->flags & TSK_SHARED_LIB)
                {
                    // we where loading a shared lib..
                    vmm_lib_loaded(task, FALSE);
                }
                else
                {
                    res_msg.status  = PM_INVALID_FILEFORMAT;
			        if(task->creator_task != 0xFFFF)
                        send_msg(task->creator_task, task->creator_task_port, &res_msg );
			
                    task->io_finished.callback = elf_fileclosed_callback;

			        /* Close the file */
			        io_begin_close(iosrc);
                }
            }
		}
	}
	return 1;
}


INT32 elf_seek_finished_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);
	struct pm_msg_response res_msg;

	res_msg.pm_type = PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;
	
	if(ioret != IO_RET_OK)
	{
        if(task->flags & TSK_SHARED_LIB)
        {
            // we where loading a shared lib..
            vmm_lib_loaded(task, FALSE);
        }
        else
        {
		    if(task->creator_task != 0xFFFF)
		    {                
			    res_msg.status  = PM_IO_ERROR;
			    send_msg(task->creator_task, task->creator_task_port, &res_msg );

                task->io_finished.callback = elf_fileclosed_callback;
			    io_begin_close(iosrc);
		    }
        }
	}
	else
	{
		elf_readphdrs(task, io_begin_read, io_begin_seek);
	}
	return 1;
}

INT32 elf_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
    // file was closed because of an error, destroy the task
    tsk_destroy(tsk_get(iosrc->id));
}
