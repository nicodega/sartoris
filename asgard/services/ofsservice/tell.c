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
			
struct stdfss_res *tell_file(int wpid, struct working_thread *thread, struct stdfss_tell *tell_cmd)
{
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct stdfss_res *ret = NULL;

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, tell_cmd->file_id, &finf, &tinf))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	if(finf->dinf->mount_info == NULL && finf->dinf->dev_type == DEVTYPE_CHAR)
	{
		// send char device tell
		// NOTE: it does not matter whether 
		// the device is blocked or not
		return chardev_tell(wpid, thread, tell_cmd, finf);
	}

	ret = build_response_msg(thread->command.command, STDFSSERR_OK);

	((struct stdfss_tell_res *)ret)->cursor = finf->cursor_possition;

	return ret;
}			
