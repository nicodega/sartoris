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
			
struct stdfss_res *mkdir(int wpid, struct working_thread *thread, struct stdfss_mkdir *mkdir_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL;
	struct sdevice_info *opening_dinf = NULL;
	char *str = NULL, *old_str = NULL, *strmatched = NULL;
	int parse_ret, dir_exists;
	struct gc_node *dir_base_node = NULL, *new_dir_base_node = NULL;
	unsigned int nodeid;

	// get dir name from smo
	str = get_string(mkdir_cmd->dir_path);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	// get mount info
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(mkdir_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		return ret;
	}

	// remove trailing '/'
	if(str[len(str)] == '/')
	{
		old_str = str;
		str = substring(str, 0, len(str) - 1);
		free(old_str);
	}

	// check file does not exist
	parse_ret = parse_directory(TRUE, &dir_base_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), NULL, &nodeid, NULL, &dir_exists, &ret);

	if(ret != NULL) free(ret);
	ret = NULL;

	free(strmatched);
	
	if(parse_ret)
	{
		// file exists
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		free(str);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		nfree(minf->dinf, dir_base_node);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_EXISTS);
	}
	if(!dir_exists)
	{
		// worng path
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		free(str);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_DOESNOTEXIST);
	}

	dir_base_node = thread->lastdir_parsed_node;
	
	// Create directory file
	if(!create_file(dir_base_node, str, last_index_of(str, '/') + 1, OFS_DIRECTORY_FILE, TRUE, minf, wpid, thread->command.command, &nodeid, &new_dir_base_node , OFS_NOFLAGS, &ret))
	{
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		free(str);
		return ret;
	}

	nfree(minf->dinf, thread->lastdir_parsed_node);
	thread->lastdir_parsed_node = NULL;
	nfree(minf->dinf, new_dir_base_node);

	free(str);


	// unlock the dir node
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
