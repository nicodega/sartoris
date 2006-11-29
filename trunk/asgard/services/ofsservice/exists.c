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

/* FileInfo implementation */
struct stdfss_res *exists(int wpid, struct working_thread *thread, struct stdfss_exists *exists_cmd)
{
	// this command will return info on a given file/dir
	struct smount_info *minf = NULL;
	struct stdfss_res *ret = NULL;
	char *str = NULL, *strmatched = NULL;
	struct gc_node *file_node = NULL;
	int flags = -1;
	unsigned int nodeid;
	int parse_value;

	str = get_string(exists_cmd->path_smo);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;
			
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(exists_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		return ret;
	}

	// parse for file info
	parse_value = parse_directory(FALSE, &file_node, OFS_NODELOCK_BLOCKING, exists_cmd->command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, NULL, NULL, &ret);

	if(!parse_value)
	{
		free(strmatched);
		free(str);
		return ret;
	}

	free(strmatched);
	free(str);

	ret = build_response_msg(exists_cmd->command, STDFSSERR_OK);

	// set file type
	if((file_node->n.type & OFS_DEVICE_FILE) == OFS_DEVICE_FILE)
	{
		((struct stdfss_exists_res *)ret)->type = STDFSS_FILETYPE_DEVICE;
	}
	else if((file_node->n.type & OFS_DIRECTORY_FILE) == OFS_DIRECTORY_FILE)
	{
		((struct stdfss_exists_res *)ret)->type = STDFSS_FILETYPE_DIRECTORY;		
	}
	else if((file_node->n.type & OFS_FILE) == OFS_FILE)
	{
		((struct stdfss_exists_res *)ret)->type = STDFSS_FILETYPE_FILE;		
	}

	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

	nfree(minf->dinf, file_node);

	return ret;
}				
