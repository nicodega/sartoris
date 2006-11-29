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

/* free nodes/blocks bitmaps handling */
unsigned int *get_free_nodes(int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	return get_bitmap_freeindexes(1, force_preserve, count, minf, command, wpid, ret);
}
unsigned int *get_free_blocks(int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	return get_bitmap_freeindexes(0, force_preserve, count, minf, command, wpid, ret);
}
void preserve_blocksbitmap_changes(int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	preserve_bitmap_changes(0, force, minf, command, wpid, ret);
}
void preserve_nodesbitmap_changes(int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	preserve_bitmap_changes(1, force, minf, command, wpid, ret);
}
void free_node(int preserve, int force_preserve, int node, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	// this function is ment to be used when deleting a file
	free_bitmap_index(1, preserve, force_preserve, node, minf, command, wpid, ret);
}
void free_block(int preserve, int force_preserve, int block, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	free_bitmap_index(0, preserve, force_preserve, block, minf, command, wpid, ret);
}
void free_bitmap_index(int nodes, int preserve, int force_preserve, int index, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	bitmap *current_bitmap = NULL;

	if(nodes)
	{
		current_bitmap = &minf->group_m_free_nodes[index / minf->ofst.nodes_per_group];
	}
	else
	{
		// NOTE: index is the block LBA
		current_bitmap = &minf->group_m_free_blocks[get_block_group(minf, index)];
	}

	wait_mutex(&minf->free_space_mutex);
	
	free_index(current_bitmap, index);

	if(nodes)
	{
		minf->nodes_modified++;
	}
	else
	{
		minf->blocks_modified++;
	}
	if(preserve)
	{
		preserve_bitmap_changes(nodes, force_preserve, minf, command, wpid, ret);
	}
	leave_mutex(&minf->free_space_mutex);
}

unsigned int *get_bitmap_freeindexes(int nodes, int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	unsigned int *indexes = NULL, *tmp_indexes = NULL;
	int free_indexes = 0;
	unsigned int i = 0, available;
	bitmap *current_bitmap = NULL;

	wait_mutex(&minf->free_space_mutex);

	if(nodes)
	{
		while(i < minf->ofst.group_count)
		{
			free_indexes += minf->group_m_free_nodes[i].freecount;
			i++;
		}
	}
	else
	{
		while(i < minf->ofst.group_count)
		{
			free_indexes += minf->group_m_free_blocks[i].freecount;
			i++;
		}
	}
	
	if(free_indexes < count)
	{
		leave_mutex(&minf->free_space_mutex);
		return NULL;
	}

	// get the nodes
	indexes = (unsigned int *)malloc(sizeof(unsigned int) * count);

	i = 0;
	while(i < minf->ofst.group_count)
	{
		if(nodes)
		{
			current_bitmap = &minf->group_m_free_nodes[i];
		}
		else
		{
			current_bitmap = &minf->group_m_free_blocks[i];
		}

		if(current_bitmap->freecount == 0)
		{
			i++;
			continue;
		}
		available = MIN(count, current_bitmap->freecount);
		
		tmp_indexes = get_free_indexes(current_bitmap, available);
		
		if(count == available)
		{
			// all indexes where available on this group //
			free(indexes);

			if(nodes)
			{
				minf->nodes_modified++;
			}
			else
			{
				minf->blocks_modified++;
			}
			
			leave_mutex(&minf->free_space_mutex);

			preserve_bitmap_changes(nodes, force_preserve, minf, command, wpid, ret);

			if(*ret != NULL)
			{
				free(tmp_indexes);
				return NULL;
			}
			return tmp_indexes;
		}
		mem_copy((unsigned char *)tmp_indexes, (unsigned char *)(indexes + count), sizeof(unsigned int) * available);
		count += available;
		free(tmp_indexes);
		i++;
	}

	if(nodes)
	{
		minf->nodes_modified++;
	}
	else
	{
		minf->blocks_modified++;
	}

	leave_mutex(&minf->free_space_mutex);

	preserve_bitmap_changes(nodes, force_preserve, minf, command, wpid, ret);

	if(ret != NULL)
	{
		free(indexes);
		return NULL;
	}

	return indexes;
}

void preserve_bitmap_changes(int nodes, int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret)
{
	unsigned int i = 0, j, block_count, last_block, first_block;
	struct stdblockdev_writem_cmd writemcmd;
	struct stdblockdev_res deviceres;
	unsigned int modified_block = -1;
	bitmap *current_bitmap = NULL;

	wait_mutex(&minf->free_space_mutex);

	if(force || minf->nodes_modified >= OFS_NODESMODIFIED_MAX)
	{
		// preserve changes on nodes/blocks bitmap modified devblocks
		// go through all group bitmaps checking for a modified devblocks
		// if any, copy block back to the disk.
		
		while(i < minf->ofst.group_count)
		{
			if(nodes)
			{
				current_bitmap = &minf->group_m_free_nodes[i];
			}
			else
			{
				current_bitmap = &minf->group_m_free_blocks[i];
			}

			if(current_bitmap->last_modified != -1)
			{
				// there are some modified blocks
				// find all consecutive modified blocks, preserve them, an so on
				j = current_bitmap->first_modified;

				modified_block = get_first_modified(current_bitmap, j, -1);

				first_block = modified_block;

				while(j <= current_bitmap->last_modified)
				{	
					if(modified_block != -1)
					{
						last_block = modified_block;
						block_count = 1;

						if(modified_block % 8 == 7){ j++; }

						modified_block = get_first_modified(current_bitmap, j, modified_block);

						while(j < current_bitmap->last_modified && (last_block + 1) == modified_block && modified_block != -1)
						{
								last_block = modified_block;
								block_count++;
								modified_block = get_first_modified(current_bitmap, j, last_block);
								if(modified_block % 8 == 7){ j++; }
						}

						// preserve all blocks found...
						writemcmd.buffer_smo = share_mem(minf->deviceid, (current_bitmap->bits + (first_block * OFS_BLOCKDEV_BLOCKSIZE)), block_count * OFS_BLOCKDEV_BLOCKSIZE , READ_PERM);
						writemcmd.command = BLOCK_STDDEV_WRITEM;
						writemcmd.dev = minf->logic_deviceid;
						writemcmd.msg_id = wpid;
						writemcmd.ret_port = OFS_BLOCKDEV_PORT;
						writemcmd.count = block_count;
						if(nodes)
						{
							writemcmd.pos = minf->group_headers[i].nodes_bitmap_offset + first_block;
						}
						else
						{
							writemcmd.pos = minf->group_headers[i].blocks_bitmap_offset + first_block;
						}
						
						locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &writemcmd, wpid);

						claim_mem(writemcmd.buffer_smo);
						
						get_signal_msg((int*)&deviceres, wpid);

						if(deviceres.ret != STDBLOCKDEV_OK)
						{
							// fix first modified
							current_bitmap->first_modified = first_block;

							*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
							return;
						}

						
						// reset modified on blocks preserved
						while(block_count > 0)
						{
							reset_modified(current_bitmap, first_block + block_count - 1);
							block_count--;
						}
						j++;

					}
					else
					{
						j++;
						modified_block = get_first_modified(current_bitmap, j, modified_block);
					}
				}
				
				// reset modified positions
				current_bitmap->first_modified = current_bitmap->modified_length; 
				current_bitmap->last_modified = -1;
			}
			i++;
		}
		
		minf->nodes_modified = 0; // reset nodes modified
	}
	leave_mutex(&minf->free_space_mutex);
}

unsigned int get_block_group(struct smount_info *minf, int block)
{
	return (unsigned int)((block - minf->ofst.first_group) / minf->group_size);
}
