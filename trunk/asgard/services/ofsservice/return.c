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
			
struct stdfss_res *begin_return(int task, struct stdfss_return *cmd, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf)
{
    struct stask_file_info *finf;
	struct stask_info *tinf;

	wait_mutex(&opened_files_mutex);

	tinf = (struct stask_info *)avl_getvalue(tasks, task);

	if(tinf == NULL)
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(cmd->command, STDFSSERR_FILE_NOTOPEN);
	}

	finf = (struct stask_file_info*)avl_getvalue(tinf->open_files, cmd->file_id);

	if(finf == NULL || finf->takeover != tinf)
	{
		leave_mutex(&opened_files_mutex);
		return build_response_msg(cmd->command, STDFSSERR_FILE_NOTOPEN);
	}
        
    finf->takeover = TAKEOVER_CLOSING;

    *dinf = finf->dinf;
	*deviceid = finf->deviceid;
	*logic_deviceid = finf->logic_deviceid;

	leave_mutex(&opened_files_mutex);
    
	return NULL;
}

struct stdfss_res *do_return(int wpid, struct working_thread *thread, struct stdfss_return *cmd)
{
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;

	wait_mutex(&opened_files_mutex);

	tinf = (struct stask_info *)avl_getvalue(tasks, thread->taskid);

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

    finf->takeover = NULL;

    avl_remove(&tinf->open_files, finf->fileid);

    if(avl_get_total(&tinf->open_files) == 0)
	{
		avl_remove(&tasks, thread->taskid);
		free(tinf);
	}

	leave_mutex(&opened_files_mutex);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}		



