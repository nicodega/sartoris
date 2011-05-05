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

	res_msg.pm_type = PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;

	/* Reading has just finished for the ELF header. */
	task = tsk_get(iosrc->id);
	
	if(ioret != IO_RET_OK)
	{
		if(task->command_inf.creator_task_id != 0xFFFF)
		{
			send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
		}
        tsk_destroy(task);
	}
	else
	{
		if(!elf_check_header(task))
		{
			res_msg.status  = PM_INVALID_FILEFORMAT;
			send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
			
            task->io_finished.callback = elf_fileclosed_callback;

			/* Close the file */
			io_begin_close(iosrc);
		}
		else
		{
			elf_seekphdrs(task, io_begin_read, io_begin_seek);
		}
	}
	return 1;
}

INT32 elf_readph_finished_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);
	struct pm_msg_response res_msg;

	res_msg.pm_type = PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;

	if(ioret != IO_RET_OK)
	{
		if(task->command_inf.creator_task_id != 0xFFFF)
		{
			send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
		}
        tsk_destroy(task);
	}
	else if(task->command_inf.creator_task_id != 0xFFFF)
	{
		if(elf_check(task))
		{				
			/* Set expected_working_set with the size of ELF segments */
			loader_calculate_working_set(task);

			/* Check created task fits in memory */
			if(vmm_can_load(task))
			{				
				// finished loading
				task->state = TSK_NORMAL;
				task->loader_inf.stage = LOADER_STAGE_LOADED;

				/* Task loaded successfully */
				res_msg.status  = PM_OK;
				res_msg.new_id = task->id;
				send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
			}
			else
			{
				tsk_destroy(task);

				/* Task cannot be loaded onto memory */
				res_msg.status  = PM_NOT_ENOUGH_MEM;
				send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
			}
		}
		else
		{
			res_msg.status  = PM_INVALID_FILEFORMAT;
			send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
			
            task->io_finished.callback = elf_fileclosed_callback;

			/* Close the file */
			io_begin_close(iosrc);
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
		if(task->command_inf.creator_task_id != 0xFFFF)
		{
			res_msg.status  = PM_IO_ERROR;
			send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );

            task->io_finished.callback = elf_fileclosed_callback;

			io_begin_close(iosrc);
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