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
			
struct stdfss_res *mkdevice(int wpid, struct working_thread *thread, struct stdfss_mkdevice *mkdevice_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL;
	struct sdevice_info *opening_dinf = NULL;
	char *str = NULL, *strmatched = NULL;
	int parse_ret, dir_exists;
	struct gc_node *dir_base_node = NULL, *device_base_node = NULL;
	unsigned int *block = NULL;
	unsigned int nodeid;
	
	// get device file name from smo
	str = get_string(mkdevice_cmd->path_smo);
	
	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	char *devstr = get_string(mkdevice_cmd->service_name);

	ret = check_path(devstr, thread->command.command);
	if(ret != NULL)
	{
		free(str);
		return ret;
	}

	// check path is ok
	if(str[len(str)] == '/')
	{
		free(str);
		free(devstr);
		return build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND_PARAMS);
	}

	// get mount info
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		free(str);
		free(devstr);
		return build_response_msg(mkdevice_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
	}

	// check file does not exist
	parse_ret = parse_directory(TRUE, &dir_base_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), NULL, &nodeid, NULL, &dir_exists, &ret);

	if(ret != NULL) free(ret);
	ret = NULL;

	if(parse_ret)
	{
		// file exists
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		free(strmatched);
		free(str);
		free(devstr);
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
		free(strmatched);
		free(str);
		free(devstr);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_DOESNOTEXIST);
	}

	dir_base_node = nref(minf->dinf, thread->lastdir_parsed_node->nodeid);
	
	free(strmatched);

	// Create device file
	if(!create_file(dir_base_node, str, last_index_of(str, '/') + 1, OFS_DEVICE_FILE, TRUE, minf, wpid, thread->command.command, &nodeid, &device_base_node, OFS_NOFLAGS , &ret))
	{
		nfree(minf->dinf, thread->lastdir_parsed_node);
		nfree(minf->dinf, dir_base_node);
		thread->lastdir_parsed_node = NULL;
		
		free(str);
		free(devstr);
		return ret;
	}
	
	nfree(minf->dinf, thread->lastdir_parsed_node);
	nfree(minf->dinf, dir_base_node);
	thread->lastdir_parsed_node = NULL;
	
	// get a free block 
	block = get_free_blocks(1, TRUE, minf, thread->command.command, wpid, &ret);
	if(ret != NULL)
	{
		free(str);
		free(devstr);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		nfree(minf->dinf, device_base_node);
		return ret;
	}

	free(str);

	clear_dir_buffer(thread->directory_buffer.buffer);

	// write buffer
	/*
		int LOGIC DEVICE ID: internal ID on the service (4 BYTES)
		int service name size; (4 BYTES)
		char SERVICE NAME[]: name of the device driver (zero terminated devstring) 
	*/
	*((unsigned int *)thread->directory_buffer.buffer) = (unsigned int)mkdevice_cmd->logic_deviceid;
	*((unsigned int *)(thread->directory_buffer.buffer + 4)) = len(devstr);
	mem_copy((unsigned char *)devstr, ((unsigned char *)thread->directory_buffer.buffer + 8), len(devstr) + 1);

	free(devstr);

	write_buffer((char *)thread->directory_buffer.buffer, OFS_DIR_BUFFERSIZE, *block, thread->command.command, wpid, minf, &ret);
	if(ret != NULL)
	{
		free_block(TRUE, TRUE, *block, minf, thread->command.command, wpid, &ret);
		free(block);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		nfree(minf->dinf, device_base_node);
		return ret;
	}

	// update node
	device_base_node->n.file_size = 8 + len(devstr) + 1;
	device_base_node->n.blocks[0] = *block;

	if(!write_node(device_base_node, minf, wpid, thread->command.command, &ret) || ret != NULL)
	{
		free_block(TRUE, TRUE, *block, minf, thread->command.command, wpid, &ret);
		free(block);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		nfree(minf->dinf, device_base_node);
		return ret;
	}

	nfree(minf->dinf, device_base_node);

	// unlock device node
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);	

	return build_response_msg(thread->command.command, STDFSSERR_OK);
}
