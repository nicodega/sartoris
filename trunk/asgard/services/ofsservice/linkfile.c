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

struct stdfss_res *link_file(int wpid, struct working_thread *thread, struct stdfss_link *link_cmd)
{
	struct smount_info *minf = NULL;
	struct stdfss_res *ret = NULL;
	char *str = NULL, *linkstr = NULL, *strmatched = NULL;
	struct gc_node *file_node = NULL;
	int flags = -1, dir_exists;
	int parse_value;
	unsigned int nodeid;

	str = get_string(link_cmd->smo_file);
	
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
		free(strmatched);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	linkstr = get_string(link_cmd->smo_link);

	ret = check_path(linkstr, thread->command.command);
	if(ret != NULL)
	{
		free(str);
		free(strmatched);
		return ret;
	}

	if(streq(str, linkstr))
	{
		free(str);
		free(linkstr);
		free(strmatched);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// check link does not exists
	parse_value = parse_directory(TRUE, &file_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, linkstr, len(strmatched), &flags, &nodeid, NULL, &dir_exists, &ret);

	if(parse_value || !dir_exists)
	{
		free(strmatched);
		free(str);
		free(linkstr);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);	
		if(parse_value) nfree(minf->dinf, file_node);
		return ret;
	}

	// lock link directory
	if(!lock_node(thread->lastdir_parsed_node->nodeid, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK, wpid, thread->command.command, &ret))
	{
		free(strmatched);
		free(str);
		free(linkstr);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		if(parse_value) nfree(minf->dinf, file_node);
	}

	if(thread->lastdir_parsed_node != NULL)
	{
		nfree(minf->dinf, thread->lastdir_parsed_node);
		thread->lastdir_parsed_node = NULL;
	}

	// parse for file info
	parse_value = parse_directory(TRUE, &file_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, NULL, NULL, &ret);

	if(!parse_value)
	{
		free(strmatched);
		free(str);
		free(linkstr);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		return ret;
	}

	free(str);
	free(strmatched);

	// createfile, sending nodeid
	if(!create_file(thread->lastdir_parsed_node, linkstr, last_index_of(linkstr, '/') + 1, file_node->n.type, TRUE, minf, wpid, thread->command.command, &nodeid, NULL, flags, &ret))
	{
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		nfree(minf->dinf, file_node);
		return ret;
	}

	free(linkstr);

	file_node->n.link_count++;

	// preserve node
	if(!write_node(file_node, minf, wpid, thread->command.command, &ret))
	{
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		nfree(minf->dinf, file_node);
		return ret;
	}

	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
	unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);

	if(thread->lastdir_parsed_node != NULL)
	{
		nfree(minf->dinf, thread->lastdir_parsed_node);
		thread->lastdir_parsed_node = NULL;
	}
	nfree(minf->dinf, file_node);

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}	
