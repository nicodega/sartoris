/*
*
*	OFS (Obsession File system) Multithreaded implementation
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
	
#include "ofs_internals.h"
			
struct stdfss_res *begin_takeover(int task, struct stdfss_takeover *cmd, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf)
{
    struct stask_file_info *finf;
	struct stask_info *tinf;

	wait_mutex(&opened_files_mutex);

	tinf = (struct stask_info *)avl_getvalue(tasks, cmd->task_id);

	if(tinf == NULL)
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(cmd->command, STDFSSERR_FILE_NOTOPEN);
	}

	finf = (struct stask_file_info*)avl_getvalue(tinf->open_files, cmd->file_id);

	if(finf == NULL || finf->takeover != NULL || (finf->mode & ~cmd->mode_mask))
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(cmd->command, STDFSSERR_FILE_NOTOPEN);
	}

    tinf = (struct stask_info *)avl_getvalue(tasks, task);

    if(tinf == NULL)
	{
		// create the task
        tinf = (struct stask_info *)malloc(sizeof(struct stask_info));
		avl_init(&tinf->open_files);

		avl_insert(&tasks, tinf, task);
	}
    
    finf->takeover = tinf;

    *dinf = finf->dinf;
	*deviceid = finf->deviceid;
	*logic_deviceid = finf->logic_deviceid;

	leave_mutex(&opened_files_mutex);
    
	return NULL;
}

struct stdfss_res *takeover(int wpid, struct working_thread *thread, struct stdfss_takeover *cmd)
{
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL, *new_owner = NULL;

	wait_mutex(&opened_files_mutex);

	tinf = (struct stask_info *)avl_getvalue(tasks, cmd->task_id);

	if(tinf == NULL)
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	finf = (struct stask_file_info*)avl_getvalue(tinf->open_files, cmd->file_id);

	if(finf == NULL || finf->takeover != NULL)
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

    new_owner = (struct stask_info *)avl_getvalue(tasks, thread->taskid);

    // we need to check the task exists in case 
    // some close operation took place and deleted the 
    // task because it had no files.
    if(new_owner == NULL)
	{
		// create the task
        new_owner = (struct stask_info *)malloc(sizeof(struct stask_info));
		avl_init(&new_owner->open_files);

		avl_insert(&tasks, new_owner, thread->taskid);
        finf->takeover = new_owner;  // update takeover pointer
	}

    // insert the finf on the open_files tree
    avl_insert(&new_owner->open_files, finf, finf->fileid);

	leave_mutex(&opened_files_mutex);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}		



