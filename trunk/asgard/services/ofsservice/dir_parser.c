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

// NOTE: inter OFS block displacement of directory names has never been tested, 
// there might be (and probably are) bugs. 
int parse_directory(int use_cache, struct gc_node **parsed_node, int lock_mode, int command, struct working_thread *thread, int wpid, struct smount_info *minf, char *path, int path_start, int *flags, unsigned int *out_nodeid, int *out_direntries, int *out_direxists, struct stdfss_res **ret)
{
	unsigned int locked_nodeid = -1;
	unsigned int nodeid = 0;
	struct gc_node *fetched_node = NULL;
	struct gc_node *first_block_node = NULL;
	struct gc_node *second_block_node = NULL;
	struct sdir_entry *dentry = NULL;
	unsigned long long total_read, file_size;
	unsigned int buffer_pos;
	int path_end, count, temp_count, tmp_start;
	char *str = NULL;
	unsigned int buffer_size;

	int dentry_first_dev_block;
	int first_block_blockdev_count;
	int second_block_blockdev_count;
	int first_block_blockdev_total_count;
	int first_block;
	int blocks_left;

	// if it's a dir, parse will also get the directory entries count
	// if out_direntries != NULL
	int calculate_entries = out_direntries != NULL;	
	int calculating_entries = 0;
	int entries = 0;

	if(out_direntries != NULL)
	{
		*out_direntries = 0;
	}
	
	second_block_blockdev_count = 0;

	first_block_node = nref(minf->dinf, 0);

	// if asked for '/' dir
	if(len(path) - path_start < 1)
	{
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}

		if(flags != NULL && *flags != -1)
		{
			// cannot change root flags
			*ret = build_response_msg(command, STDFSSERR_FILE_INVALIDOPERATION);
			return FALSE;
		}

		if(flags != NULL) *flags = OFS_READ_ONLY_FILE;
		if(out_nodeid != NULL) *out_nodeid = 0; // root node id
		
		if(!calculate_entries)
		{
			if(parsed_node != NULL) *parsed_node = first_block_node;

			// TODO: CHECK IF ROOT NODE SHOULD BE LOCKED
			if(!lock_node(0, lock_mode, OFS_LOCKSTATUS_DENYALL, wpid, command, ret))
			{
				return FALSE;
			}
			locked_nodeid = 0;

			return TRUE;
		}

		if(first_block_node->n.blocks[0] == OFS_NULL_VALUE || first_block_node->n.file_size == 0)
		{
			// sought file is the root, direntries = 0
			if(out_direntries != NULL) *out_direntries = 0;
			if(parsed_node != NULL) *parsed_node = first_block_node;

			return TRUE;
		}

		// calculate dir entries
		calculating_entries = 1;
	}
	
	if(path[path_start] == '/' && !calculating_entries)
	{
		path_start++;
	}

	if(first_block_node->n.blocks[0] == OFS_NULL_VALUE || first_block_node->n.file_size == 0)
	{
		// If dir_exists is required, check if the path
		// represents a file on the root
		if(last_index_of(path, '/') == path_start - 1)
		{
			if(out_direxists != NULL) *out_direxists = TRUE;
			
			if(use_cache)
			{
				if(thread->lastdir_parsed_node == NULL)
				{
					thread->lastdir_parsed_node = nref(minf->dinf, 0);
					thread->lastdir_parent_nodeid = -1;
					thread->lastdir_parsed_start = path_start;
				}
			}
		}
		else
		{
			if(thread->lastdir_parsed_node != NULL) 
			{
				nfree(minf->dinf, thread->lastdir_parsed_node);
				thread->lastdir_parsed_node = NULL;
				thread->lastdir_parsed_start = -1;
				thread->lastdir_parent_nodeid = -1;
			}
		}

		nfree(minf->dinf, first_block_node);

		*ret = build_response_msg(command, STDFSSERR_FILE_DOESNOTEXIST);
		return FALSE;
	}
	
	// assume directory does not exist
	if(out_direxists != NULL && !calculating_entries) *out_direxists = FALSE;

	// attempt using path cache
#ifdef OFS_PATHCACHE
	// if flags is not null we won't use the cache
	if(((flags != NULL && *flags == -1) || flags == NULL) && !calculating_entries)
	{
		struct gc_node *parent_cachedn = NULL;
		unsigned int cflags;
		unsigned int dir_parentid;

		struct gc_node *cachedn = get_cached_node(&minf->dinf->pcache, path, &cflags, &parent_cachedn, &dir_parentid);

		if(cachedn != NULL)
		{
			if(!use_cache)
			{
				if(thread->lastdir_parsed_node != NULL)
				{
					nfree(minf->dinf, thread->lastdir_parsed_node);
					thread->lastdir_parsed_node = NULL;
					thread->lastdir_parsed_start = -1;
					thread->lastdir_parent_nodeid = -1;
				}
			}
			else
			{
				// ok using old style cache
				// if thread->lastdir_parsed_node is NULL, we set it to the parent node.
				thread->lastdir_parsed_node = nref(minf->dinf, parent_cachedn->nodeid);
				thread->lastdir_parent_nodeid = dir_parentid;
				// calculate path start
				int c = 0, ln = len(path);
				while(ln > 0 && c < 2)
				{
					if(path[ln] == '/') c++;
					ln--;
				}
				thread->lastdir_parsed_start = c+1;
			}

			// if flags are not being modified and we are not calculating dir entries
			// return cached info
			if(out_direntries == NULL || !(cachedn->n.type & OFS_DIRECTORY_FILE))
			{
				if(!lock_node(dentry->nodeid, lock_mode, (parent_cachedn == NULL)? OFS_LOCKSTATUS_DENYALL : OFS_LOCKSTATUS_DENY(parent_cachedn->nodeid), wpid, command, ret))
				{
					//if(nodeid != 0) free_node_buffer(nodeid);
					nfree(minf->dinf, cachedn);
					nfree(minf->dinf, parent_cachedn);
					nfree(minf->dinf, first_block_node);
					*ret = build_response_msg(command, STDFSSERR_FILE_INUSE);
					return FALSE;
				}

				nfree(minf->dinf, parent_cachedn);
				nfree(minf->dinf, first_block_node);

				// ok we can safely return here ^^			
				if(flags != NULL) *flags = cflags;
				if(out_nodeid != NULL) *out_nodeid = cachedn->nodeid;
				if(out_direxists != NULL) *out_direxists = TRUE;
				if(parsed_node != NULL) *parsed_node = cachedn;

				return TRUE;
			}
			else
			{
				// prepare for counting dir entries
				nfree(minf->dinf, first_block_node);
				nfree(minf->dinf, parent_cachedn);
				first_block_node = cachedn;

				// use cached node
				path_start = last_index_of(path, '/')+1;

				// lock cached dir node for parsing
				if(!lock_node(thread->lastdir_parsed_node->nodeid, OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_DENYALL, wpid, command, ret))
				{
					nfree(minf->dinf, cachedn);
					return FALSE;
				}

				locked_nodeid = cachedn->nodeid;

				if(flags != NULL) *flags = cflags;
				if(out_nodeid != NULL) *out_nodeid = cachedn->nodeid;
				if(out_direxists != NULL) *out_direxists = TRUE;
				if(parsed_node != NULL) *parsed_node = cachedn;
				if(out_direntries != NULL) *out_direntries = 0;	
					
				// see if we reached the end of file
				if(first_block_node->n.blocks[0] == OFS_NULL_VALUE || first_block_node->n.file_size == 0)
				{
					return TRUE;
				}

				calculating_entries = 1;
			}
		}
	}
#endif
	
	if(use_cache &&  !calculating_entries)
	{		
		if(thread->lastdir_parsed_node == NULL)
		{
			thread->lastdir_parsed_node = nref(minf->dinf, first_block_node->nodeid);
			thread->lastdir_parent_nodeid = -1;
			thread->lastdir_parsed_start = path_start;

			// lock mount dir node for parsing
			if(!lock_node(0, OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_DENYALL, wpid, command, ret))
			{
				nfree(minf->dinf, first_block_node);
				return FALSE;
			}
		}
		else
		{
			nfree(minf->dinf, first_block_node);

			// use cached node
			path_start = thread->lastdir_parsed_start;

			// lock cached dir node for parsing
			if(!lock_node(thread->lastdir_parsed_node->nodeid, OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, (thread->lastdir_parent_nodeid == -1 )? OFS_LOCKSTATUS_DENYALL : OFS_LOCKSTATUS_DENY(thread->lastdir_parent_nodeid), wpid, command, ret))
			{
				return FALSE;
			}

			first_block_node = nref(minf->dinf, thread->lastdir_parsed_node->nodeid);
		}
		locked_nodeid = thread->lastdir_parsed_node->nodeid;
	}
	else if(!calculating_entries)
	{
		// lock mount dir node for parsing
		if(!lock_node(0, OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_DENYALL, wpid, command, ret))
		{
			nfree(minf->dinf, first_block_node);
				
			return FALSE;
		}
		locked_nodeid = 0;
	}

	file_size = first_block_node->n.file_size;
	first_block_blockdev_count = OFS_BLOCKDEVBLOCK_SIZE;
	first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
	first_block = 0;
	buffer_pos = 0;
	buffer_size = MIN((unsigned int)file_size, OFS_BLOCK_SIZE);
	total_read = MIN(file_size, OFS_BLOCK_SIZE);

	// start dir buffer filling on first block
	if(!bc_read((char*)thread->directory_buffer.buffer, OFS_DIR_BUFFERSIZE, first_block_node->n.blocks[0], command, wpid, minf, TRUE, ret))
	{
		if(locked_nodeid != -1)
		{
			unlock_node(locked_nodeid, TRUE, OFS_LOCKSTATUS_OK);
		}
		nfree(minf->dinf, first_block_node);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
		second_block_node = NULL;
		return FALSE;
	}

	do
	{
		// parse buffer for dir entry (or direntry count calculation)
		while(buffer_pos < buffer_size)
		{
			// calculate ending char of the name for the directory being parsed
			path_end = first_index_of(path, '/', path_start);
			
			if(path_end == -1)
			{
				path_end = len(path);
			}

			path_end--; // remove trailing '/'

			// check a record fits on remaining buffer space
			if(OFS_BLOCK_SIZE - buffer_pos > sizeof(dir_entry))
			{
				dentry = (struct sdir_entry *)(thread->directory_buffer.buffer + buffer_pos);

				// check there's enough size for the record on the buffer
				if(OFS_BLOCK_SIZE - buffer_pos >= dentry->record_length)
				{
					if(calculating_entries)
					{
						// if entry is not deleted, count it
						if((dentry->flags & OFS_DELETED_FILE) != OFS_DELETED_FILE) 
							entries++;
					}
					else
					{
						// ignore deleted entries, and check length is the same
						if((dentry->flags & OFS_DELETED_FILE) != OFS_DELETED_FILE && dentry->name_length == (path_end - path_start + 1))
						{
							// compare name
							str = (char *)(thread->directory_buffer.buffer + buffer_pos + sizeof(struct sdir_entry));

							if(istrprefix(path_start, str, path))
							{
								// name is a prefix and length is the same -> direntry is the one being sought
								tmp_start = first_index_of(path, '/', path_start);

								unlock_node(wpid, TRUE, OFS_LOCKSTATUS_OK);		

								// node found, lock it for parsing
								if(!lock_node(dentry->nodeid, ((tmp_start == -1 || tmp_start == len(path))? lock_mode : (OFS_NODELOCK_BLOCKING | OFS_NODELOCK_DIRECTORY)), (thread->lastdir_parent_nodeid == -1 )? OFS_LOCKSTATUS_DENYALL : OFS_LOCKSTATUS_DENY(thread->lastdir_parent_nodeid), wpid, command, ret))
								{
									nfree(minf->dinf, first_block_node);
									if(thread->lastdir_parsed_node != NULL)
									{
										nfree(minf->dinf, thread->lastdir_parsed_node);
										thread->lastdir_parsed_node = NULL;
									}
									if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
									second_block_node = NULL;
									*ret = build_response_msg(command, STDFSSERR_FILE_INUSE);
									return FALSE;
								}

								locked_nodeid = dentry->nodeid;
								
								// to check if a file does have the node open
								// is unnecesary for it's a directory (only opened on read mode)
								// or it's a file, and open already checked it was not already opened
								// or being parsed

								if(!read_node(&fetched_node, dentry->nodeid,minf, wpid, command, ret))
								{
									if(locked_nodeid != -1)
									{
										unlock_node(locked_nodeid, ((tmp_start == -1 || tmp_start == len(path))? ((lock_mode & OFS_NODELOCK_DIRECTORY)? TRUE : FALSE) : TRUE), OFS_LOCKSTATUS_OK);
									}
									nfree(minf->dinf, first_block_node);
									if(thread->lastdir_parsed_node != NULL)
									{
										nfree(minf->dinf, thread->lastdir_parsed_node);
										thread->lastdir_parsed_node = NULL;
									}
									if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
									second_block_node = NULL;
									*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
									return FALSE;
								}

#ifdef OFS_PATHCACHE
								char c = path[path_end+1];
								path[path_end+1] = '\0';

								cache_path_node(&minf->dinf->pcache, path, dentry->nodeid, dentry->flags);
								path[path_end+1] = c;
#endif
								
								// save current path start for updating cache info
								tmp_start = path_start;
								path_start = first_index_of(path, '/', path_start);
								
								// check if it's the end of the path being parsed
								if(path_start == -1 || path_start == len(path) - 1)
								{									
									// file was found on this directory, preserve flags if needed
									if(flags != NULL && *flags != -1)
									{
										// there can be two blocks on the buffer
										// both of them can be on the same node
										// or one of them can be on other node
										// if dentry is on both blocks, write it properly
										
										dentry->flags = *flags;

										// calculate the first devblock on buffer for dentry
										dentry_first_dev_block = (int)(buffer_pos / OFS_BLOCKDEV_BLOCKSIZE);

										// see if dentry is on one or two devblocks
										count = ((OFS_BLOCKDEV_BLOCKSIZE - (buffer_pos % OFS_BLOCKDEV_BLOCKSIZE)) >= sizeof(struct sdir_entry))? 1: 2;
										
										temp_count = count;
		
										// preserve first block part of dentry (this can be only on first node)
										if(dentry_first_dev_block < first_block_blockdev_count)
										{
											// see how many blocks of dentry are on first block
											temp_count = (dentry_first_dev_block + 1 == first_block_blockdev_count)? 1 : count;
											if(!bc_write((char*)(thread->directory_buffer.buffer + dentry_first_dev_block * OFS_BLOCKDEV_BLOCKSIZE), 
											temp_count * OFS_BLOCKDEV_BLOCKSIZE, 
											first_block_node->n.blocks[first_block] + (first_block_blockdev_total_count - first_block_blockdev_count) + dentry_first_dev_block, 
											command, wpid, minf, TRUE, ret))
											{
												if(locked_nodeid != -1)
												{
													unlock_node(locked_nodeid, ((lock_mode & OFS_NODELOCK_DIRECTORY)? TRUE : FALSE), OFS_LOCKSTATUS_OK);
												}
												nfree(minf->dinf, first_block_node);
												if(thread->lastdir_parsed_node != NULL)
												{
													nfree(minf->dinf, thread->lastdir_parsed_node);
													thread->lastdir_parsed_node = NULL;
												}
												if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
												nfree(minf->dinf, fetched_node);
									
												return FALSE;
											}

											count -= temp_count;
										}
										
										// preserve second block part (it might be on other node)
										if(count > 0)
										{
											unsigned int wlba = 0;
											if(second_block_node != NULL)
											{
												wlba = second_block_node->n.blocks[0];
											}
											else
											{
												wlba = first_block_node->n.blocks[first_block + 1];
											}
											wlba += ((dentry_first_dev_block < first_block_blockdev_count)? 0 : dentry_first_dev_block - first_block_blockdev_count);

											if(!bc_write((char*)(thread->directory_buffer.buffer + (dentry_first_dev_block + (count - temp_count)) * OFS_BLOCKDEV_BLOCKSIZE), temp_count * OFS_BLOCKDEV_BLOCKSIZE, wlba, command, wpid, minf, TRUE, ret ))
											{
											
												if(locked_nodeid != -1)
												{
													unlock_node(locked_nodeid, ((lock_mode & OFS_NODELOCK_DIRECTORY)? TRUE : FALSE), OFS_LOCKSTATUS_OK);
												}
												nfree(minf->dinf, first_block_node);
												if(thread->lastdir_parsed_node != NULL)
												{
													nfree(minf->dinf, thread->lastdir_parsed_node);
													thread->lastdir_parsed_node = NULL;
												}
												if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
												nfree(minf->dinf, fetched_node);
									
												return FALSE;
											}
										}

									}

									if(flags != NULL) *flags = dentry->flags;
									if(out_nodeid != NULL) *out_nodeid = locked_nodeid;
									
									if(out_direxists != NULL) *out_direxists = TRUE;
									if(parsed_node != NULL)
									{
										*parsed_node = nref(minf->dinf, fetched_node->nodeid); // FIXED: was: fetched_node, but if parsed_node was NULL a reference to the node was kept.
									}
									
									// if not calculating entries return TRUE
									if(!calculate_entries || (calculate_entries && ((fetched_node->n.type & OFS_FILE) || (fetched_node->n.type & OFS_DEVICE_FILE))))
									{
										// free the nodes here, because if we are calculating dir entries
										// later these will be dereferenced a second time
										nfree(minf->dinf, first_block_node);
										if(second_block_node != NULL) free(second_block_node);

										nfree(minf->dinf, fetched_node); // FIXED: This had to be added because of previous fix.

										return TRUE;
									}
									
									// begin entries calculation for the directory
									calculating_entries = 1;
									entries = 0;
									path_start = tmp_start - 1; //restore pathstart for entries calculation
								}

								// begin reading next dir //

								path_start++; // remove the '/'

								// check the fetched node is a directory (if it's not we cant continue)
								if(!(fetched_node->n.type & OFS_DIRECTORY_FILE))
								{
									if(locked_nodeid != -1)
									{
										unlock_node(locked_nodeid, TRUE, OFS_LOCKSTATUS_OK);
									}
									if(first_block_node->n.blocks[0] == OFS_NULL_VALUE)
									{
										// file is not on this directory
										*ret = build_response_msg(command, STDFSSERR_FILE_DOESNOTEXIST);
									}
									else
									{
										*ret = build_response_msg(command, STDFSSERR_INVALID_COMMAND);
									}
									nfree(minf->dinf, fetched_node);
									if(thread->lastdir_parsed_node != NULL)
									{
										nfree(minf->dinf, thread->lastdir_parsed_node);
										thread->lastdir_parsed_node = NULL;
									}
									if(second_block_node != NULL) nfree(minf->dinf, second_block_node);

									return FALSE;
								}
								
								// update cache info with current dir
								if(use_cache && !calculating_entries)
								{
									thread->lastdir_parsed_start = path_start;
									nfree(minf->dinf, thread->lastdir_parsed_node);
									thread->lastdir_parsed_node = nref(minf->dinf, fetched_node->nodeid);
									thread->lastdir_parent_nodeid = locked_nodeid;
								}

								nfree(minf->dinf, first_block_node);
								first_block_node = fetched_node;
								fetched_node = NULL;

								// free second node, for there is a dir match
								if(second_block_node != NULL)
								{
									nfree(minf->dinf, second_block_node);
									second_block_node = NULL;
								}
								second_block_blockdev_count = 0;
							
								// if file is not empty read first block of next file
								if(first_block_node->n.blocks[0] != OFS_NULL_VALUE)
								{
									file_size = first_block_node->n.file_size;
									first_block_blockdev_count = OFS_BLOCKDEVBLOCK_SIZE;
									first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
									first_block = 0;
									buffer_pos = 0;
									buffer_size = MIN((unsigned int)file_size, OFS_BLOCK_SIZE);
									total_read = MIN(file_size, OFS_BLOCK_SIZE);

									if(!bc_read((char*)thread->directory_buffer.buffer, OFS_DIR_BUFFERSIZE, first_block_node->n.blocks[0], command, wpid, minf, TRUE, ret))
									{
										if(locked_nodeid != -1)
										{
											unlock_node(locked_nodeid, TRUE, OFS_LOCKSTATUS_OK);
										}
										nfree(minf->dinf, first_block_node);
										if(thread->lastdir_parsed_node != NULL)
										{
											nfree(minf->dinf, thread->lastdir_parsed_node);
											thread->lastdir_parsed_node = NULL;
										}
										if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
										second_block_node = NULL;
										
										return FALSE;
									}
								}
								else
								{
									file_size = 0;
									first_block_blockdev_count = OFS_BLOCKDEVBLOCK_SIZE;
									first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
									first_block = 0;
									buffer_pos = 0;
									buffer_size = 0;
									total_read = 0;
								}

								// new dir first block was fetched, continue parsing with new buffer
								continue;
							}
						}
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				break; // exit this loop so buffer filling continues
			}

			// step onto next dir entry
			buffer_pos += dentry->record_length;
		}
		// not found check reason and begin again with reading
		
		// check EOF
		if(total_read == file_size)
		{
			if(calculating_entries)
			{
				if(second_block_node != NULL) free(second_block_node);
				if(out_direntries != NULL) *out_direntries = entries;
				if(out_direxists != NULL) *out_direxists = TRUE;
				return TRUE;
			}
			else
			{
				// if we got to the containing directory but it doesnt have the file
				// dont delete the caching info if any and set out_direxists to TRUE
				if(out_direxists != NULL && path_start == last_index_of(path, '/') + 1)
				{
					if(locked_nodeid != -1)
					{
						unlock_node(locked_nodeid, OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK);
					}
					nfree(minf->dinf, first_block_node);
											
					if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
					*out_direxists = TRUE;
					*ret = build_response_msg(command, STDFSSERR_FILE_DOESNOTEXIST);
					return FALSE;
				}
				if(locked_nodeid != -1)
				{
					unlock_node(locked_nodeid, OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK);
				}
				if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
				*ret = build_response_msg(command, STDFSSERR_FILE_DOESNOTEXIST);
				return FALSE;
			}
		}

	
		// if buffer was not read completely copy blocks to the lower part of the buffer				
		if(buffer_pos != buffer_size)
		{
			mem_copy(thread->directory_buffer.buffer + buffer_pos - (buffer_pos % OFS_BLOCKDEV_BLOCKSIZE), thread->directory_buffer.buffer, OFS_BLOCK_SIZE - (buffer_pos - (buffer_pos % OFS_BLOCKDEV_BLOCKSIZE)) );
			
			// calculate blocks left (not parsed) on buffer
			blocks_left = (OFS_BLOCK_SIZE - (buffer_pos - (buffer_pos % OFS_BLOCKDEV_BLOCKSIZE))) / OFS_BLOCKDEV_BLOCKSIZE;

			if(second_block_blockdev_count >= blocks_left)
			{
				// all data left is on second_block
				// then it becomes the first block and second vanishes
				if(second_block_node != NULL)
				{
					nfree(minf->dinf, first_block_node);
					first_block_node = second_block_node;
					second_block_node = NULL;
				}
				first_block_blockdev_total_count = second_block_blockdev_count;
				first_block_blockdev_count = blocks_left;
				second_block_blockdev_count = 0;

				first_block++;
				if(first_block == PTRS_ON_NODE){ first_block = 0; }
			}
			else
			{
				// part of the first block was left on buffer
				first_block_blockdev_count = blocks_left - second_block_blockdev_count;
			}
			
			buffer_pos = buffer_pos % OFS_BLOCKDEV_BLOCKSIZE;
		}
		else
		{
			// buffer was read completely
			// see if there is a second block
			if(second_block_blockdev_count != 0)
			{
				// buffer was fully read, first block is complete
				// set second block to first block
				if(second_block_node != NULL)
				{
					nfree(minf->dinf, first_block_node);
					first_block_node = second_block_node;
					second_block_node = NULL;
				}
				first_block_blockdev_total_count = second_block_blockdev_count;
				first_block_blockdev_count = 0;
				second_block_blockdev_count = 0;

				first_block++;
				if(first_block == PTRS_ON_NODE){ first_block = 0; }
			}
			else
			{
				first_block_blockdev_count = 0;
			}
			
			buffer_pos = 0;
		}
		
		// set size of buffer
		buffer_size = (first_block_blockdev_count + second_block_blockdev_count) * OFS_BLOCKDEV_BLOCKSIZE;

		// fetch next node if necesary
		if(second_block_node == NULL && second_block_blockdev_count == 0 && first_block == (PTRS_ON_NODE - 1) && first_block_blockdev_total_count != first_block_blockdev_count)
		{
			fetched_node = NULL;

			if(!read_node(&fetched_node, first_block_node->n.next_node, minf, wpid, command, ret))
			{
				if(locked_nodeid != -1)
				{
					unlock_node(locked_nodeid, OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK);
				}
				nfree(minf->dinf, first_block_node);
				if(thread->lastdir_parsed_node != NULL)
				{
					nfree(minf->dinf, thread->lastdir_parsed_node);
					thread->lastdir_parsed_node = NULL;
				}
				if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
				*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
				return FALSE;
			}

			nodeid = first_block_node->n.next_node;
			
			// we got the node on the buffer
			
			// check if node is to be placed on first or second block
			if(first_block_blockdev_total_count == OFS_BLOCKDEVBLOCK_SIZE)
			{
				nfree(minf->dinf, first_block_node);
				first_block_node = fetched_node;
			}
			else
			{
				second_block_node = fetched_node;
			}
		}

		
		// try to fill buffer with first block
		if(second_block_blockdev_count == 0)
		{
			int retval = 0;
			unsigned int rcount = 0;
			if(first_block_blockdev_total_count == OFS_BLOCKDEVBLOCK_SIZE && first_block_blockdev_count == 0)
			{
				first_block++;
				retval = bc_read((char*)thread->directory_buffer.buffer, OFS_DIR_BUFFERSIZE, first_block_node->n.blocks[first_block], command, wpid, minf, TRUE, ret);
				rcount = OFS_DIR_BUFFERSIZE;
				first_block_blockdev_count = OFS_BLOCKDEVBLOCK_SIZE;
				first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
			}
			else if(first_block_blockdev_total_count != OFS_BLOCKDEVBLOCK_SIZE && first_block_blockdev_count != 0)
			{
				rcount = (OFS_BLOCKDEVBLOCK_SIZE - first_block_blockdev_total_count) * OFS_BLOCKDEV_BLOCKSIZE;
				
				// part of the first block is still to be read but some of it remains on buffer
				retval = bc_read((char*)
					(thread->directory_buffer.buffer + first_block_blockdev_count * OFS_BLOCKDEV_BLOCKSIZE),
					rcount, 	
					first_block_node->n.blocks[first_block] + first_block_blockdev_total_count, command, wpid, 
					minf, TRUE, ret);
				first_block_blockdev_count += (OFS_BLOCKDEVBLOCK_SIZE - first_block_blockdev_total_count);
				first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
			}
			else if(first_block_blockdev_total_count != OFS_BLOCKDEVBLOCK_SIZE && first_block_blockdev_count == 0)
			{
				rcount =(OFS_BLOCKDEVBLOCK_SIZE - first_block_blockdev_total_count) * OFS_BLOCKDEV_BLOCKSIZE;
				
				// a part of the first block was read but nothing remains on buffer
				retval = bc_read((char*)thread->directory_buffer.buffer, 
						rcount,
						first_block_node->n.blocks[first_block] + first_block_blockdev_total_count, 
						command, wpid, minf, TRUE, ret);
				first_block_blockdev_count += (OFS_BLOCKDEVBLOCK_SIZE - first_block_blockdev_total_count);
				first_block_blockdev_total_count = OFS_BLOCKDEVBLOCK_SIZE;
			}
			
			total_read = MIN(file_size, total_read + rcount);
			buffer_size += rcount;
			
			if(!ret)
			{
				if(locked_nodeid != -1)
				{
					unlock_node(locked_nodeid, OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK);
				}
				nfree(minf->dinf, first_block_node);
				if(thread->lastdir_parsed_node != NULL)
				{
					nfree(minf->dinf, thread->lastdir_parsed_node);
					thread->lastdir_parsed_node = NULL;
				}
				if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
				
				return FALSE;
			}
		}

		// fetch second block if there's still some space left on buffer
		if(first_block_blockdev_count != OFS_BLOCKDEVBLOCK_SIZE)
		{
			int retval = 0;
			unsigned int rcount = 0;
			if(second_block_blockdev_count != 0)
			{
				unsigned int readp = 0;
				if(second_block_node != NULL)
				{
					readp = second_block_node->n.blocks[0] + second_block_blockdev_count;
				}
				else
				{
					readp = first_block_node->n.blocks[first_block + 1] + second_block_blockdev_count;
				}
				rcount = (OFS_DIR_BUFFERSIZE - (first_block_blockdev_count + second_block_blockdev_count) * OFS_BLOCKDEV_BLOCKSIZE);
				// there is a part of block2 on buffer
				retval = bc_read((char*)(thread->directory_buffer.buffer + (first_block_blockdev_count + second_block_blockdev_count) * OFS_BLOCKDEV_BLOCKSIZE),
							rcount, 
							readp, command, wpid, minf, TRUE, ret);
			}
			else
			{
				unsigned int readp = 0;

				if(second_block_node != NULL)
				{
					readp = second_block_node->n.blocks[0];
				}
				else
				{
					readp = first_block_node->n.blocks[first_block + 1];
				}
				rcount = (OFS_DIR_BUFFERSIZE - first_block_blockdev_count * OFS_BLOCKDEV_BLOCKSIZE);
				// block2 has never been written
				retval = bc_read((char*)(thread->directory_buffer.buffer + first_block_blockdev_count * OFS_BLOCKDEV_BLOCKSIZE),
							rcount, 
							readp, command, wpid, minf, TRUE, ret);
			}

			second_block_blockdev_count += rcount / OFS_BLOCKDEV_BLOCKSIZE;
			total_read = MIN(file_size, total_read + second_block_blockdev_count * OFS_BLOCKDEV_BLOCKSIZE);
			buffer_size += OFS_BLOCKDEV_BLOCKSIZE * second_block_blockdev_count;
			
			if(!retval)
			{
				if(locked_nodeid != -1)
				{
					unlock_node(locked_nodeid, OFS_NODELOCK_DIRECTORY, OFS_LOCKSTATUS_OK);
				}
				nfree(minf->dinf, first_block_node);
				if(thread->lastdir_parsed_node != NULL)
				{
					nfree(minf->dinf, thread->lastdir_parsed_node);
					thread->lastdir_parsed_node = NULL;
				}
				if(second_block_node != NULL) nfree(minf->dinf, second_block_node);
				
				return FALSE;
			}
		}
		
	}while(1); // cycle will exit on eof or found internally with a break statement

	// we will never get to this point 

	return FALSE;
}

		
