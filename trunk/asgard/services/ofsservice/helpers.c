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

int check_opened_file(int fileid, int taskid, int base_nodeid, int *open_mode)
{
	CPOSITION it;
	struct stask_file_info *finf = NULL;

	wait_mutex(&opened_files_mutex);
	it = get_head_position(&opened_files);
	
	while(it != NULL)
	{
		finf = (struct stask_file_info *)get_next(&it);
		if((finf->fileid == fileid && finf->taskid == taskid && finf->file_base_node->nodeid == base_nodeid) || (fileid = -1 && finf->file_base_node->nodeid == base_nodeid))
		{
			leave_mutex(&opened_files_mutex);
			if(open_mode != NULL) *open_mode = finf->mode;
			return TRUE;
		}
	}
	leave_mutex(&opened_files_mutex);
	return FALSE;
}


struct stdfss_res *check_path(char *path, int command)
{
	if(path == NULL) return build_response_msg(command, STDFSSERR_INVALID_PATH);

	int i = 0, ln = len(path);
	while(i < ln)
	{
		if(!lpt_is_valid(path[i]))
		{			
			free(path);
			return build_response_msg(command, STDFSSERR_INVALID_PATH);
		}
		i++;
	}
	return NULL;
}
// gets info of a device from the mount tree.
int get_device_info(struct stdfss_cmd *cmd,int taskid, int fileid, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf, struct stdfss_res **ret)
{
	char *str = NULL, *old_str = NULL;
	struct smount_info *minf =  NULL, *link_minf = NULL;
	task_file_info *tfinf = NULL;
	task_info *tinf = NULL;
	int parcial = 0;
	*ret = NULL;

	switch(cmd->command)
	{
		case STDFSS_UMOUNT:
			str = get_string(((struct stdfss_umount*)cmd)->mount_path);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			// remove trailing / 
			if(last_index_of(str,'/') == len(str) - 1)
			{
				old_str = str;
				str = substring(str, 0, len(str) - 1);
				free(old_str);
			}
			parcial = 0;
			break;
		case STDFSS_MOUNT:
			str = get_string(((struct stdfss_mount*)cmd)->dev_path);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_OPEN:
			str = get_string(((struct stdfss_open*)cmd)->file_path);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1; // file cannot exist, and might be created
			break;
		case STDFSS_DELETE:
			str = get_string(((struct stdfss_delete*)cmd)->path_smo);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_EXISTS:
			str = get_string(((struct stdfss_exists*)cmd)->path_smo);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_FILEINFO:
			str = get_string(((struct stdfss_fileinfo*)cmd)->path_smo);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_CHANGEATT:
			str = get_string(((struct stdfss_changeatt *)cmd)->file_path);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_MKDIR:
			str = get_string(((struct stdfss_mkdir *)cmd)->dir_path);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_MKDEVICE:
			str = get_string(((struct stdfss_mkdevice *)cmd)->path_smo);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			parcial = 1;
			break;
		case STDFSS_LINK:
			str = get_string(((struct stdfss_link*)cmd)->smo_link);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			
			link_minf = (struct smount_info *)lpt_getvalue_parcial_match(mounted, str);
			
			free(str);
			
			if(link_minf == NULL) return FALSE;
		
			str = get_string(((struct stdfss_link*)cmd)->smo_file);
			*ret = check_path(str, cmd->command);
			if(*ret != NULL) return FALSE;
			
			minf = (struct smount_info *)lpt_getvalue_parcial_match(mounted, str);
			
			free(str);
			
			if(minf == NULL) return FALSE;
			
			if((minf->deviceid != link_minf->deviceid) || (minf->logic_deviceid != link_minf->logic_deviceid)) return FALSE;
			
			*dinf = minf->dinf;
			*deviceid = minf->deviceid;
			*logic_deviceid = minf->logic_deviceid;
			
			return TRUE;
		case STDFSS_CLOSE:
		case STDFSS_SEEK:
		case STDFSS_TELL:
		case STDFSS_READ:
		case STDFSS_WRITE:
		case STDFSS_FLUSH:
		case STDFSS_IOCTL:
		case STDFSS_PUTS:
		case STDFSS_PUTC:
		case STDFSS_GETC:
		case STDFSS_GETS:
			// find the device and logic_dev using task_id and file_id //
			if(taskid == -1 || fileid == -1) return FALSE;
			
			tinf = (task_info *)avl_getvalue(tasks, taskid);
			
			if(tinf == NULL) return FALSE;
			
			tfinf = (task_file_info *)avl_getvalue(tinf->open_files, fileid);
			
			if(tfinf == NULL) return FALSE;			

			*dinf = tfinf->dinf;
			*deviceid = tfinf->deviceid;
			*logic_deviceid = tfinf->logic_deviceid;
			
			return TRUE;
	}
	
	*ret = check_path(str, cmd->command);
	if(*ret != NULL) return FALSE;
	
	wait_mutex(&mounted_mutex);
	{
		// check the mounted tree //
		if(parcial)
		{
			minf = (struct smount_info *)lpt_getvalue_parcial_match(mounted, str);
		}
		else
		{
			minf = (struct smount_info *)lpt_getvalue(mounted, str);
		}
	}
	leave_mutex(&mounted_mutex);

	free(str);
	
	if(minf == NULL)
	{
		return FALSE;
	}

	*dinf = minf->dinf;
	*deviceid = minf->deviceid;
	*logic_deviceid = minf->logic_deviceid;

	return TRUE;
}



// gets file info for a fileid of a given task
int get_file_info(int taskid, int fileid, struct stask_file_info **out_finf, struct stask_info **out_tinf)
{
	struct stask_file_info *finf;
	struct stask_info *tinf;

	wait_mutex(&opened_files_mutex);

	tinf = (struct stask_info *)avl_getvalue(tasks, taskid);

	if(tinf == NULL)
	{
		leave_mutex(&opened_files_mutex);
		return FALSE;
	}

	finf = (struct stask_file_info *)avl_getvalue(tinf->open_files, fileid);

	if(finf == NULL)
	{
		leave_mutex(&opened_files_mutex);
		return FALSE;
	}

	if(tinf != NULL) *out_tinf = tinf;
	if(finf != NULL) *out_finf = finf;

	leave_mutex(&opened_files_mutex);

	return TRUE;
}

int read_buffer(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, struct stdfss_res **ret )
{
	return bc_read(buffer, buffer_size, lba, command, wpid, minf, FALSE, ret);
}

int write_buffer(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, struct stdfss_res **ret )
{
	return bc_write(buffer, buffer_size, lba, command, wpid, minf, FALSE, ret);
}
