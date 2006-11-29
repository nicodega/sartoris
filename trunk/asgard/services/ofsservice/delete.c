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

/* Delete implementation */
struct stdfss_res *delete_file(int wpid, struct working_thread *thread, struct stdfss_delete *delete_cmd)
{
	struct stdfss_res *ret = NULL;
	struct smount_info *minf = NULL;
	char *str = NULL, *strmatched = NULL;
	struct gc_node *file_node = NULL, *base_node = NULL;
	int i;
	int flags = -1;
	unsigned int nodeid, oldnodeid;
	int parse_ret;
	int dir_entries;

	str = get_string(delete_cmd->path_smo);

	ret = check_path(str, thread->command.command);
	if(ret != NULL) return ret;

	wait_mutex(&mounted_mutex);
	minf = (struct smount_info *)lpt_getvalue_parcial_matchE(mounted, str, &strmatched);
	leave_mutex(&mounted_mutex);

	if(minf == NULL)
	{
		ret = build_response_msg(delete_cmd->command, STDFSSERR_DEVICE_NOT_MOUNTED);
		free(str);
		return ret;
	}

	if(len(strmatched) == len(str))
	{
		free(str);
		free(strmatched);
		return build_response_msg(delete_cmd->command, STDFSSERR_FILE_INVALIDOPERATION);
	}

	// parse for file node caching
	parse_ret = parse_directory(TRUE, &file_node, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, &dir_entries, NULL, &ret);

	// if it's a dir, check it has no entries
	if(parse_ret && file_node->n.type == OFS_DIRECTORY_FILE && dir_entries > 0)
	{
		ret = build_response_msg(thread->command.command, STDFSSERR_FILE_DIRNOTEMPTY);
		// unlock the node
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		// free parsed node reference
		nfree(minf->dinf, file_node);

		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
	}

	if(!parse_ret || ret != NULL)
	{
		free(str);
		free(strmatched);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		return ret;
	}

	// check if file is open and fail if it is
	if(check_opened_file(-1, -1, nodeid, NULL)) 
	{
		// file is open, fail
		free(str);
		free(strmatched);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		// free parsed node reference
		nfree(minf->dinf, file_node);
		return build_response_msg(thread->command.command, STDFSSERR_FILE_INUSE);
	}

	// decrement node count
	base_node = nref(minf->dinf, file_node->nodeid); // if read node succeds it'll remove the reference
	base_node->n.link_count--;

	// free the node, and all it's blocks :'(
	if(base_node->n.link_count == 0)
	{
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
						free(str);
						free(strmatched);
						unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
						if(thread->lastdir_parsed_node != NULL)
						{
							nfree(minf->dinf, thread->lastdir_parsed_node);
							thread->lastdir_parsed_node = NULL;
						}
						// free parsed node reference
						nfree(minf->dinf, file_node);
						nfree(minf->dinf, base_node);
						return ret;
					}
				}
				else
				{
					break;
				}
				i++;
			}

			oldnodeid = nodeid;	

			// check if next node must be fetched
			if(file_node->n.next_node != OFS_NULL_VALUE)
			{
				nodeid = file_node->n.next_node;
				// get next node
				if(!read_node(&file_node, file_node->n.next_node, minf, wpid, thread->command.command, &ret))
				{
					free(str);
					free(strmatched);
					unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
					if(thread->lastdir_parsed_node != NULL)
					{
						nfree(minf->dinf, thread->lastdir_parsed_node);
						thread->lastdir_parsed_node = NULL;
					}
					nfree(minf->dinf, file_node);
					nfree(minf->dinf, base_node);
					return ret;
				}	
			}

			// free node from bitmap
			free_node(FALSE, FALSE, oldnodeid, minf, thread->command.command, wpid, &ret);
		}
		while(file_node->n.next_node != OFS_NULL_VALUE);
	}

	nfree(minf->dinf, file_node);
					
	// set flags on dir using cached parsed node ;)
	flags |= OFS_DELETED_FILE;

	parse_ret = parse_directory(TRUE, NULL, OFS_NODELOCK_EXCLUSIVE | OFS_NODELOCK_BLOCKING, thread->command.command, thread, wpid, minf, str, len(strmatched), &flags, &nodeid, NULL, NULL, &ret);

	// check if last parsing went ok
	if(ret != NULL)
	{
		free(str);
		free(strmatched);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		// free parsed node reference
		nfree(minf->dinf, base_node);
		return ret;
	}

	// write node back to disk
	if(!write_node(base_node, minf, wpid, thread->command.command, &ret) || ret != NULL)
	{
		free(str);
		free(strmatched);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		nfree(minf->dinf, base_node);
		return ret;
	}

	nfree(minf->dinf, base_node);

	// preserve bitmap changes
	preserve_blocksbitmap_changes(TRUE, minf, thread->command.command, wpid, &ret);
	if(ret != NULL)
	{
		free(str);
		free(strmatched);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		return ret;
	}
	preserve_nodesbitmap_changes(TRUE, minf, thread->command.command, wpid, &ret);
	if(ret != NULL)
	{
		free(str);
		free(strmatched);
		unlock_node(wpid, FALSE, OFS_LOCKSTATUS_OK);
		if(thread->lastdir_parsed_node != NULL)
		{
			nfree(minf->dinf, thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}
		return ret;
	}

	
	// when parsed and deleted free the NODE
	// after parsing thread->lastdir_parsed_node->nodeid holds the dir base node id
	// of the node parsed. Deny
	unlock_node(wpid, FALSE, OFS_LOCKSTATUS_DENY(thread->lastdir_parsed_node->nodeid));

#ifdef OFS_PATHCACHE
	invalidate_cache_path(&minf->dinf->pcache, str);
#endif

	free(str);
	free(strmatched);

	// free parse cache node
	if(thread->lastdir_parsed_node != NULL)
	{
		nfree(minf->dinf, thread->lastdir_parsed_node);
		thread->lastdir_parsed_node = NULL;
	}
	
	return build_response_msg(thread->command.command, STDFSSERR_OK);
}	
