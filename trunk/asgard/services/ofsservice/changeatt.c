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
			

	
struct stdfss_res *change_attributes(int wpid, struct working_thread *thread, struct stdfss_changeatt *changeatt_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL;
	char *str = NULL, *strmatched = NULL;
	int flags = OFS_NOFLAGS;
	unsigned int nodeid;
	int parse_ret;

	str = get_string(changeatt_cmd->file_path);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		return ret;
	}

	if(len(strmatched) == len(str))
	{
		free(str);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// map flags from stdfss to OFS
	if(changeatt_cmd->flags & STDFSS_ATT_READONLY)
	{
		if(changeatt_cmd->substract)
			flags &= !OFS_READ_ONLY_FILE;
		else
			flags |= OFS_READ_ONLY_FILE;
	}
	if(changeatt_cmd->flags & STDFSS_ATT_HIDDEN)
	{
		if(changeatt_cmd->substract)
			flags &= !OFS_HIDDEN_FILE;
		else
			flags |= OFS_HIDDEN_FILE;
	}

	// check DELETED is not being set
	if(flags & OFS_DELETED_FILE)
	{
		// fail
		free(str);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	parse_ret = parse_directory(FALSE, NULL, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, NULL, NULL, &ret);

	if(!parse_ret)
	{
		free(str);
		free(strmatched);
		return ret;
	}

	// unlock the node
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
