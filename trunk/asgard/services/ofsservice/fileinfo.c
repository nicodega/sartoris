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
struct stdfss_res *file_info(int wpid, struct working_thread *thread, struct stdfss_fileinfo *fileinfo_cmd)
{
	// this command will return info on a given file/dir
	struct smount_info *minf = NULL;
	struct stdfss_res *ret = NULL;
	char *str = NULL, *strmatched = NULL;
	struct file_info0 fileinfo_struct;
	struct gc_node *file_node;
	int flags = -1;
	unsigned int nodeid;
	int parse_value;
	int dir_entries;

	str = get_string(fileinfo_cmd->path_smo);
		
	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(fileinfo_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		return ret;
	}

	// parse for file info
	parse_value = parse_directory(FALSE, &file_node, OFS_NODELOCK_BLOCKING, fileinfo_cmd->command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, &dir_entries, NULL, &ret);

	if(!parse_value)
	{
		free(strmatched);
		free(str);
		return ret;
	}

	fileinfo_struct.creation_date = file_node->n.creation_date;
	fileinfo_struct.modification_date = file_node->n.modification_date;
	fileinfo_struct.dir_entries = dir_entries;
	fileinfo_struct.hard_links = file_node->n.link_count;
	fileinfo_struct.owner_group_id = file_node->n.owner_group_id;
	fileinfo_struct.owner_id = file_node->n.owner_id;
	fileinfo_struct.protection = file_node->n.protection_mode;
	fileinfo_struct.file_size = file_node->n.file_size;
	
	if((file_node->n.type & OFS_DEVICE_FILE) == OFS_DEVICE_FILE)
	{
		fileinfo_struct.file_type = STDFSS_FILETYPE_DEVICE;

		// get device info
		if(!read_device_file(file_node, minf, wpid, thread->command.command, &fileinfo_struct.device_service_id, &fileinfo_struct.logic_device_id, &ret))
		{
			free(strmatched);
			free(str);
			nfree(minf->dinf, file_node);
			return ret;
		}
	}
	else if((file_node->n.type & OFS_DIRECTORY_FILE) == OFS_DIRECTORY_FILE)
	{
		fileinfo_struct.file_type = STDFSS_FILETYPE_DIRECTORY;
	}
	else if((file_node->n.type & OFS_FILE) == OFS_FILE)
	{
		fileinfo_struct.file_type = STDFSS_FILETYPE_FILE;
	}

	if(flags & OFS_READ_ONLY_FILE)
	{
		fileinfo_struct.flags = STDFSS_ATT_READONLY;
	}
	if(flags & OFS_HIDDEN_FILE)
	{
		fileinfo_struct.flags |= STDFSS_ATT_HIDDEN;
	}
	
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);

	nfree(minf->dinf, file_node);

	// write file info on smo
	write_mem(fileinfo_cmd->file_info_smo, 0, sizeof(struct file_info0), (char*)&fileinfo_struct);

	free(strmatched);
	free(str);

	return build_response_msg(fileinfo_cmd->command, STDFSSERR_OK);
}				
