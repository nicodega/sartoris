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


/* Open implementation */
struct stdfss_res *open_file(int wpid, struct working_thread *thread, struct stdfss_open *open_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL;
	struct stask_file_info *finf = NULL, *file_inf = NULL;
	struct stask_info *tinf = NULL;
	struct sdevice_info *opening_dinf = NULL;
	struct gc_node *file_node = NULL;
	struct stdfss_open_res *open_res = NULL;
	char *str = NULL, *strmatched = NULL;
	unsigned int nodeid, wnodeid, oldnodeid;
	int file_flags = -1;
	int i;
	int parse_ret, dir_exists;
	int open_mode = 0, opened_file_mode = 0;
	int device_request_mode = 0;
	int opening_deviceid, opening_logic_deviceid;
	list mount_structures;
	CPOSITION it;

	// check max opened files
	if(thread->taskid != PMAN_TASK)
	{
		wait_mutex(&opened_files_mutex);
		{
			tinf = (struct stask_info *)avl_getvalue(tasks, thread->taskid);
			if(tinf != NULL && avl_get_total(&tinf->open_files) == OFS_MAXTASK_OPENFILES)
			{
				leave_mutex(&opened_files_mutex);
				return build_response_msg(thread->command.command, STDFSSERR_TOOMANY_FILESOPEN);
			}
		}	
		leave_mutex(&opened_files_mutex);
	}

	open_mode = open_cmd->open_mode;

	// files oppened for append won't
	if((open_mode & STDFSS_FILEMODE_WRITE) && (open_mode & STDFSS_FILEMODE_APPEND))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
	}
	
	str = get_string(open_cmd->file_path);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;
			
	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(open_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		free(strmatched);
		return ret;
	}

	if(str[len(str) - 1] == '/' && open_mode != STDFSS_FILEMODE_READ)
	{
		// directories can only be opened on read
		// so you can get file names :)
		free(str);
		free(strmatched);
		return build_response_msg(open_cmd->command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// check mount mode for read or write 
	if(((minf->mode & STDFSS_MOUNTMODE_READ) == 0 && (open_mode & STDFSS_FILEMODE_READ)) || 
		((minf->mode & STDFSS_MOUNTMODE_WRITE) == 0 && ((open_mode & STDFSS_FILEMODE_WRITE) || (open_mode & STDFSS_FILEMODE_APPEND))))
	{
		free(str);
		free(strmatched);
		return build_response_msg(open_cmd->command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// setup new task_fileinfo
	finf = (struct stask_file_info *)malloc(sizeof(struct stask_file_info));
	finf->assigned_buffer = NULL;
	finf->deviceid = minf->deviceid;
	finf->logic_deviceid = minf->logic_deviceid;
	finf->dinf = minf->dinf;
	finf->file_base_node = NULL;
	finf->current_node = NULL;
	finf->current_block = 0;
	finf->mode = open_mode;
	finf->cursor_possition = 0;
	finf->taskid = thread->taskid;
	finf->file_modified = FALSE; // not modified
	finf->buffered = ((open_cmd->flags & STDFSS_OPEN_WRITEBUFFERED)? TRUE : FALSE);
	
	// parse for file node
	parse_ret = parse_directory(TRUE, &finf->file_base_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), &file_flags, &nodeid, NULL, &dir_exists, &ret);

	free(str);
	free(strmatched);

	if(!parse_ret)
	{
		// continue only if appending or writing and directory for the file exists
		// the file will be created on the dir
		if(!dir_exists || (!(open_mode & STDFSS_FILEMODE_WRITE) && !(open_mode & STDFSS_FILEMODE_APPEND)) || (open_cmd->flags & STDFSS_FILEMODE_MUSTEXIST))
		{
			// FAIL
			free(finf);
			if(thread->lastdir_parsed_node != NULL)
			{
				nfree(minf->dinf, thread->lastdir_parsed_node);
				thread->lastdir_parsed_node = NULL;
			}
			return ret;
		}

		if(ret != NULL) free(ret);
		ret = NULL;

		// Create the file
		if(!create_file(thread->lastdir_parsed_node, str, last_index_of(str, '/') + 1, OFS_FILE, /*!finf->buffered*/ TRUE, minf, wpid, thread->command.command, &nodeid, &finf->file_base_node , OFS_NOFLAGS, &ret))
		{
			free(finf);
			free(finf->file_base_node);
			return ret;
		}

		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}

		// insert opened file structure
		// ok we can open the file
		wait_mutex(&opened_files_mutex);
		{
			// create task info
			tinf = (struct stask_info *)avl_getvalue(tasks, thread->taskid);
			if(tinf == NULL)
			{
				tinf = (struct stask_info *)malloc(sizeof(struct stask_info));
				//tinf->last_fileid = 0;
				avl_init(&tinf->open_files);

				avl_insert(&tasks, tinf, thread->taskid);
			}

			// complete file info
			finf->fileid = get_file_id(tinf, finf);
			
			// insert fileinfo
			avl_insert(&tinf->open_files, finf, finf->fileid);
			add_tail(&opened_files, finf);
		}	
		leave_mutex(&opened_files_mutex);

		// unlock the node
		if((open_mode & STDFSS_FILEMODE_EXCLUSIVE))
		{
			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_DENYALL);
		}
		else
		{
			unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		}
	}
	else
	{
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}

		if((finf->file_base_node->n.type & OFS_DIRECTORY_FILE) && open_mode != STDFSS_FILEMODE_READ)
		{
			// directories can only be opened for reading
			// so you can get file names :)
			free(str);
			free(strmatched);
			free(finf);
			nfree(minf->dinf, finf->file_base_node);
			return build_response_msg(open_cmd->command, STDFSSERR_FILE_INVALIDOPERATION);
		}

		if(((open_mode & STDFSS_FILEMODE_WRITE) || (open_mode & STDFSS_FILEMODE_APPEND)) && (file_flags & OFS_READ_ONLY_FILE))
		{
			// fail, for file is read only
			free(str);
			free(strmatched);
			free(finf);
			nfree(minf->dinf, finf->file_base_node);
			return build_response_msg(open_cmd->command, STDFSSERR_FILE_INVALIDOPERATION);
		}

		// check if it's a file or a device file
		if(!(finf->file_base_node->n.type & OFS_DEVICE_FILE))
		{
			// check read_only
			if((file_flags & OFS_READ_ONLY_FILE) && ((open_mode & STDFSS_FILEMODE_WRITE) || (open_mode & STDFSS_FILEMODE_APPEND)))
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
				free(finf);
				nfree(minf->dinf, finf->file_base_node);
				return build_response_msg(thread->command.command, STDFSSERR_FILE_INVALIDOPERATION);
			}

			// check it's not open
			if(check_opened_file(-1, -1, nodeid, &opened_file_mode))
			{   
                // it's already opened, check it can be opened again
				
				// files already opened for writing will only be available for reading.
				if((opened_file_mode & STDFSS_FILEMODE_WRITE) && (open_mode & STDFSS_FILEMODE_WRITE))
				{
					unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
					free(finf);
					nfree(minf->dinf, finf->file_base_node);
					return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
				}

				// check it can be opened with the required mode
				if((open_mode & STDFSS_FILEMODE_EXCLUSIVE) || (opened_file_mode & STDFSS_FILEMODE_EXCLUSIVE))
				{
					unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
					free(finf);
					nfree(minf->dinf, finf->file_base_node);
					return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
				}
			}
			else
			{
				// complete file info
				//finf->file_base_node->nodeid = nodeid;
			
				// if file was opened for writing and no other process has it for 
				// reading, al contents are removed
				if((open_mode & STDFSS_FILEMODE_WRITE))
				{
					file_node = nref(finf->dinf, finf->file_base_node->nodeid);
					wnodeid = finf->file_base_node->nodeid;

					// free all it's blocks :'(
					do
					{
						// free node blocks
						i = 0;
						while(i < PTRS_ON_NODE)
						{
							if(file_node->n.blocks[i] != OFS_NULL_VALUE)
							{
								// free the block
								free_block(FALSE, FALSE, file_node->n.blocks[i], minf, thread->command.command, wpid, &ret);

								if(ret != NULL)
								{
									unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
									free(finf);
									return build_response_msg(thread->command.command, STDFSSERR_FATAL);
								}

								file_node->n.blocks[i] = OFS_NULL_VALUE;
							}
							else
							{
								break;
							}
							i++;
						}

						// check if next node must be fetched
						if(file_node->n.next_node != OFS_NULL_VALUE)
						{
							oldnodeid = wnodeid;
							nodeid = file_node->n.next_node;
							// get next node
							if(!read_node(&file_node, file_node->n.next_node, minf, wpid, thread->command.command, &ret))
							{
								unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
								free(finf);
								return build_response_msg(thread->command.command, STDFSSERR_FATAL);
							}
							// free old node
							if(finf->file_base_node->nodeid != wnodeid)
								free_node(FALSE, FALSE, wnodeid, minf, thread->command.command, wpid, &ret);
							nfree(minf->dinf, file_node);
						}
					}
					while(file_node->n.next_node != OFS_NULL_VALUE);

					nfree(minf->dinf, file_node);
				
					// change file size
					finf->file_base_node->n.file_size = 0;
					finf->file_modified = TRUE; // set it as modified.. so close preserves changes

					i=0;
					while(i < PTRS_ON_NODE)
					{
						finf->file_base_node->n.blocks[i] = OFS_NULL_VALUE;
						i++;
					}

					// NOTE: if not buffered we don't preserve the file 
					// or bitmap status for it will be preserved later on close
					if(!finf->buffered)
					{
						if(!write_node(finf->file_base_node, finf->dinf->mount_info, wpid, thread->command.command, &ret))
						{
							unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
							free(finf);
							nfree(minf->dinf, finf->file_base_node);
							return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
						}

						preserve_blocksbitmap_changes(TRUE, finf->dinf->mount_info, thread->command.command, wpid, &ret);
						if(ret != NULL)
						{
							unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
							free(finf);
							nfree(minf->dinf, finf->file_base_node);
							return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
						}
						preserve_nodesbitmap_changes(TRUE, finf->dinf->mount_info, thread->command.command, wpid, &ret);
						if(ret != NULL)
						{
							unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
							free(finf);
							nfree(minf->dinf, finf->file_base_node);
							return build_response_msg(thread->command.command, STDFSSERR_DEVICE_ERROR);
						}
					}
				}
			}
			
			// ok we can open the file
			wait_mutex(&opened_files_mutex);
			{
				// create task info
				tinf = (struct stask_info *)avl_getvalue(tasks, thread->taskid);
				if(tinf == NULL)
				{
					tinf = (struct stask_info *)malloc(sizeof(struct stask_info));
					//tinf->last_fileid = 0;
					avl_init(&tinf->open_files);

					avl_insert(&tasks, tinf, thread->taskid);
				}

				finf->fileid = get_file_id(tinf, finf);

				// insert fileinfo
				avl_insert(&tinf->open_files, finf, finf->fileid);
				add_tail(&opened_files, finf);
	
			}	
			leave_mutex(&opened_files_mutex);

			// unlock the node
			if((open_mode & STDFSS_FILEMODE_EXCLUSIVE))
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_DENYALL);
			}
			else
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
			}
		}
		else
		{
			// it's a device file

			// see if the device file is already opened by a task
			wait_mutex(&opened_files_mutex);
			{
				it = get_head_position(&opened_files);

				while(it != NULL)
				{
					file_inf = (struct stask_file_info *)get_next(&it);
					if(file_inf->dinf->mount_info != NULL && file_inf->dinf->device_nodeid == nodeid)
					{
						if((file_inf->mode & STDFSS_FILEMODE_EXCLUSIVE) || (open_mode & STDFSS_FILEMODE_EXCLUSIVE))
						{
							leave_mutex(&opened_files_mutex);
							unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
							free(finf);
							nfree(minf->dinf, finf->file_base_node);
							return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
						}
						
						break;
					}
				}
			}
			leave_mutex(&opened_files_mutex);

			// get device info from file
			if(!read_device_file(finf->file_base_node, minf, wpid, thread->command.command, &opening_deviceid, &opening_logic_deviceid, &ret))
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
				free(finf);
				nfree(minf->dinf, finf->file_base_node);
				return ret;
			}
	
			// lock device
			lock_device(wpid, opening_deviceid, opening_logic_deviceid);
			
			// check it's not mounted
			wait_mutex(&mounted_mutex);
			{
				mount_structures = lpt_getvalues(mounted);
			
				it = get_head_position(&mount_structures);

				while(it != NULL)
				{
					minf = (struct smount_info *)get_next(&it);
					if(minf->deviceid == opening_deviceid && minf->logic_deviceid == opening_logic_deviceid)
					{
						// fail, for the device is already mounted
						unlock_device(wpid);
						unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
						free(finf);
						nfree(minf->dinf, finf->file_base_node);
						leave_mutex(&mounted_mutex);
						return build_response_msg(thread->command.command, STDFSSERR_DEVICE_MOUNTED);
					}
				}

				lstclear(&mount_structures);
			}
			leave_mutex(&mounted_mutex);

			wait_mutex(&opened_files_mutex);
			{
				// see if the device is already open
				it = get_head_position(&opened_files);

				while(it != NULL)
				{
					file_inf = (struct stask_file_info *)get_next(&it);
					if(file_inf->dinf->mount_info == NULL && file_inf->deviceid == opening_deviceid && file_inf->logic_deviceid == opening_logic_deviceid)
					{
						if((file_inf->mode & STDFSS_FILEMODE_EXCLUSIVE) || (open_mode & STDFSS_FILEMODE_EXCLUSIVE))
						{
							unlock_device(wpid);
							unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
							
							free(finf);
							leave_mutex(&opened_files_mutex);
							return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
						}
						// increment dinf open_count just in case the other file is closing
						file_inf->dinf->open_count++;
						opening_dinf = file_inf->dinf;
						nfree(minf->dinf, finf->file_base_node);
						break;
					}
				}
			}

			// attempt getting the device from the cache
			if(opening_dinf == NULL)
			{
				opening_dinf = get_cache_device_info(opening_deviceid, opening_logic_deviceid);

				
				if(opening_dinf != NULL)
				{
					opening_dinf->open_count++;
				}
				else
				{
					opening_dinf = (struct sdevice_info *)malloc(sizeof(struct sdevice_info));
					init_logic_device(opening_dinf, nodeid, FALSE);
					opening_dinf->open_count = 1;
					opening_dinf->device_nodeid = nodeid;
	
					// insert device on cache
					cache_device_info(opening_deviceid, opening_logic_deviceid, opening_dinf);
				}
			}

			leave_mutex(&opened_files_mutex);
				
			// set finf device to the one opened
			finf->dinf = opening_dinf;
				
			// request device depending on the open mode
			if( open_mode & STDFSS_FILEMODE_EXCLUSIVE )
			{
				device_request_mode = OFS_DEVICEREQUEST_EXCLUSIVE;
			}

			// request device
			if(opening_dinf->open_count == 1 && !request_device(wpid, opening_deviceid, opening_logic_deviceid, opening_dinf, device_request_mode, thread->command.command, &ret))
			{	
				unlock_device(wpid);
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
				
				free(finf);

				opening_dinf->open_count--;

				nfree(minf->dinf, finf->file_base_node);
				
				return ret;
			}

			finf->block_offset = 0;
			finf->dev_writecount = 0;
			finf->logic_deviceid = opening_logic_deviceid;
			finf->deviceid = opening_deviceid;
			
			wait_mutex(&opened_files_mutex);
			{
				// create task info
				tinf = (struct stask_info *)avl_getvalue(tasks, thread->taskid);
				if(tinf == NULL)
				{
					tinf = (struct stask_info *)malloc(sizeof(struct stask_info));
					//tinf->last_fileid = 0;
					avl_init(&tinf->open_files);

					avl_insert(&tasks, tinf, thread->taskid);
				}

				// complete file info
				finf->file_base_node->nodeid = nodeid;
				finf->fileid = get_file_id(tinf, finf);
				
				// insert fileinfo
				avl_insert(&tinf->open_files, finf, finf->fileid);
				add_tail(&opened_files, finf);
			}
			leave_mutex(&opened_files_mutex);

			// we shouldn't need the device file base node anymore
			nfree(minf->dinf, finf->file_base_node);
			finf->file_base_node = NULL;

			// unlock the device
			unlock_device(wpid);

			// unlock the node
			if((open_mode & STDFSS_FILEMODE_EXCLUSIVE))
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_DENYALL);
			}
			else
			{
				unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
			}
		}
	}

	// build open ret message
	open_res = (struct stdfss_open_res *)malloc(sizeof(struct stdfss_open_res));
	open_res->command = open_cmd->command;
	open_res->file_id = finf->fileid;
	open_res->ret = STDFSSERR_OK;

	return (struct stdfss_res*)open_res;
}			
