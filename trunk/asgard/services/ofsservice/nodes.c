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

int read_node(struct gc_node **fetched_node, unsigned int nodeid, struct smount_info *minf, int wpid, int command, struct stdfss_res **ret)
{
	struct stdblockdev_read_cmd breadcmd;
	unsigned char *ptr = NULL;
	struct node_buffer *nodebuffer = NULL;
	struct stdblockdev_res device_res;
	struct gc_node *gcn = NULL;

	if(fetched_node == NULL) 
	{
		*ret = build_response_msg(command, STDFSSERR_FATAL);
		return FALSE;
	}

	/* check node is not the one already sent */
	if(*fetched_node != NULL && (*fetched_node)->nodeid == nodeid) return TRUE;

	// get a buffer for node fetch
	nodebuffer = get_node_buffer(nodeid, wpid, minf->dinf);

	// see if node is cached on the device
	wait_mutex(&minf->dinf->nodes_mutex);

	gcn = (struct gc_node *)avl_getvalue(minf->dinf->nodes, nodeid);

	if(gcn != NULL)
	{
		// we can use the cached node
		if(*fetched_node != NULL)
		{
			nfree(minf->dinf, *fetched_node);
		}
		*fetched_node = gcn;
		gcn->ref++;	// increment node reference count

		leave_mutex(&minf->dinf->nodes_mutex);
		free_node_buffer(nodeid, minf->dinf);
		return TRUE;
	}

	// we can release this mutex here for nobody will ask for the same node
	// because the buffer is locked
	// NOTE: this can be ensured by the fact that all nodes read operations
	// are performed through this function. Only new nodes can be added
	// without using this function.
	leave_mutex(&minf->dinf->nodes_mutex);

	// if the buffer contains the same block, use it!
	if(nodebuffer->first_node_id != nodeid - (nodeid % OFS_NODESPER_BLOCKDEV_BLOCK))
	{

		// get the node to the buffer
		breadcmd.buffer_smo = share_mem(minf->deviceid, nodebuffer->buffer, OFS_BLOCKDEV_BLOCKSIZE, WRITE_PERM);
		breadcmd.command = BLOCK_STDDEV_READ;
		breadcmd.dev = minf->logic_deviceid;
		breadcmd.msg_id = wpid;
		breadcmd.ret_port = OFS_BLOCKDEV_PORT;
		breadcmd.pos = (int)(minf->group_headers[nodeid / minf->ofst.nodes_per_group].nodes_table_offset);
		breadcmd.pos += (unsigned int)(nodeid - (minf->ofst.nodes_per_group * (unsigned int)(nodeid / minf->ofst.nodes_per_group))) / OFS_NODESPER_BLOCKDEV_BLOCK; 

		locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &breadcmd, wpid);

		get_signal_msg((int *)&device_res, wpid);

		claim_mem(breadcmd.buffer_smo);

		if(device_res.ret == STDBLOCKDEV_ERR)
		{
			free_node_buffer(nodeid, minf->dinf);
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
			return FALSE;
		}

		// set buffer first node
		nodebuffer->first_node_id = (nodeid - (nodeid % OFS_NODESPER_BLOCKDEV_BLOCK));
	}

	
	// create gc_node struct 
	if(*fetched_node != NULL)
	{
		nfree(minf->dinf, *fetched_node);
	}

	*fetched_node = (struct gc_node*)malloc(sizeof(struct gc_node));
	(*fetched_node)->ref = 1;
	(*fetched_node)->nodeid = nodeid;

	// we got the node on the buffer copy it
	ptr = nodebuffer->buffer + (int)(nodeid % OFS_NODESPER_BLOCKDEV_BLOCK) * sizeof(struct node);
	
	mem_copy(ptr, (unsigned char *)&(*fetched_node)->n, sizeof(struct node));

	// here we enter the mutex so we won't collide with ourselves, nor with
	// create file or write functions.
	// Deadlock cannot happen because the nodes buffer mutex will only be 
	// entered by us.
	wait_mutex(&minf->dinf->nodes_mutex);
	avl_insert(&minf->dinf->nodes, *fetched_node, nodeid);
	leave_mutex(&minf->dinf->nodes_mutex);
	
	free_node_buffer(nodeid, minf->dinf);

	
	
	return TRUE;
}

int write_node(struct gc_node *n, struct smount_info *minf, int wpid, int command, struct stdfss_res **ret)
{
	struct stdblockdev_write_cmd bwritecmd;
	struct stdblockdev_read_cmd breadcmd;
	unsigned char *ptr = NULL;
	struct node_buffer *nodebuffer = NULL;
	struct stdblockdev_res device_res;
	int i = 0; // sacar

	// get a buffer for the node
	nodebuffer = get_node_buffer(n->nodeid, wpid, minf->dinf);

	// if no node is present on the buffer, or a different one is present
	// fill the node buffer before preserving it
	if(nodebuffer->first_node_id != (n->nodeid - (n->nodeid % OFS_NODESPER_BLOCKDEV_BLOCK)))
	{
		// get the node and it's neighbours to the buffer
		breadcmd.buffer_smo = share_mem(minf->deviceid, nodebuffer->buffer, OFS_BLOCKDEV_BLOCKSIZE, WRITE_PERM);
		breadcmd.command = BLOCK_STDDEV_READ;
		breadcmd.dev = minf->logic_deviceid;
		breadcmd.msg_id = wpid;
		breadcmd.ret_port = OFS_BLOCKDEV_PORT;
		breadcmd.pos = (unsigned int)(minf->group_headers[n->nodeid / minf->ofst.nodes_per_group].nodes_table_offset);
		breadcmd.pos += (unsigned int)(n->nodeid - (minf->ofst.nodes_per_group * (unsigned int)(n->nodeid / minf->ofst.nodes_per_group))) / OFS_NODESPER_BLOCKDEV_BLOCK; 

		locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &breadcmd, wpid);

		get_signal_msg((int *)&device_res, wpid);

		claim_mem(breadcmd.buffer_smo);

		if(device_res.ret == STDBLOCKDEV_ERR)
		{
			free_node_buffer(n->nodeid, minf->dinf);
			*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
			return FALSE;
		}

		// set buffer first node
		nodebuffer->first_node_id = (n->nodeid - (n->nodeid % OFS_NODESPER_BLOCKDEV_BLOCK));

	}

	// copy node to buffer
	ptr = nodebuffer->buffer + (unsigned int)(n->nodeid % OFS_NODESPER_BLOCKDEV_BLOCK) * sizeof(struct node);
	
	mem_copy((unsigned char *)&n->n, ptr, sizeof(struct node));

	// get the node to the buffer
	bwritecmd.buffer_smo = share_mem(minf->deviceid, nodebuffer->buffer, OFS_BLOCKDEV_BLOCKSIZE, READ_PERM);
	bwritecmd.command = BLOCK_STDDEV_WRITE;
	bwritecmd.dev = minf->logic_deviceid;
	bwritecmd.msg_id = wpid;
	bwritecmd.ret_port = OFS_BLOCKDEV_PORT;
	bwritecmd.pos = (int)(minf->group_headers[n->nodeid / minf->ofst.nodes_per_group].nodes_table_offset);
	bwritecmd.pos += (unsigned int)(n->nodeid - (minf->ofst.nodes_per_group * (unsigned int)(n->nodeid / minf->ofst.nodes_per_group))) / OFS_NODESPER_BLOCKDEV_BLOCK; 

	locking_send_msg(minf->deviceid, minf->dinf->protocol_port, &bwritecmd, wpid);

	get_signal_msg((int *)&device_res, wpid);

	claim_mem(bwritecmd.buffer_smo);

	if(device_res.ret == STDBLOCKDEV_ERR)
	{
		*ret = build_response_msg(command, STDFSSERR_DEVICE_ERROR);
		return FALSE;
	}

	free_node_buffer(n->nodeid, minf->dinf);

	return TRUE;
}

/* node buffer handling */
// node buffers should be used only to fetch or preserve
// a node, and must be released imediatelly after that- 
struct node_buffer *get_node_buffer(int nodeid, int wpid, struct sdevice_info *dinf)
{
	int hash = node_buffer_hash(dinf, nodeid);
	int i = 0;
	struct node_buffer *buffer = NULL;

	buffer = dinf->node_buffers[hash];
	
	wait_mutex(&buffer->buffer_mutex);

	return buffer;
}


void free_node_buffer(int nodeid, struct sdevice_info *dinf)
{
	int hash = node_buffer_hash(dinf, nodeid);

	leave_mutex(&dinf->node_buffers[hash]->buffer_mutex);
}

// compute hash for nodes
int node_buffer_hash(struct sdevice_info *dinf, int nodeid)
{
	// we will use the block possition % OFS_NODEBUFFERS
	return ((int)(dinf->mount_info->group_headers[nodeid / dinf->mount_info->ofst.nodes_per_group].nodes_table_offset) + (unsigned int)(nodeid - (dinf->mount_info->ofst.nodes_per_group * (unsigned int)(nodeid / dinf->mount_info->ofst.nodes_per_group))) / OFS_NODESPER_BLOCKDEV_BLOCK) % OFS_NODEBUFFERS; 
}

/* Nodes garbage collection. Now nodes will be shared among files, and WPs. So write
   Changes have effect when reading */

/* This function will have to be issued when a node pointer is
   no longer used, instead of m_free */
void nfree(struct sdevice_info *dinf, struct gc_node *n)
{
	struct gc_node *gcn = NULL;

	/* enter the device lock 
	   and get the node from the nodes avl
	*/

	if(n == NULL) return;

	wait_mutex(&dinf->nodes_mutex);

	gcn = (struct gc_node *)avl_getvalue(dinf->nodes, n->nodeid);

	if(gcn == NULL || gcn != n)
	{
		// nothing to be done for the node is not cached
		leave_mutex(&dinf->nodes_mutex);
		return;
	}

	gcn->ref--;

	if(gcn->ref == 0)
	{
		// we can free the node
		avl_remove(&dinf->nodes, n->nodeid);
		free(gcn);
	}
	
	leave_mutex(&dinf->nodes_mutex);
}


struct gc_node *nref(struct sdevice_info *dinf, unsigned int nodeid)
{
	struct gc_node *gcn = NULL;

	/* enter the device lock 
	   and get the node from the nodes avl
	*/

	wait_mutex(&dinf->nodes_mutex);

	gcn = (struct gc_node *)avl_getvalue(dinf->nodes, nodeid);

	if(gcn == NULL)
	{
		// nothing to be done for the node is not cached
		leave_mutex(&dinf->nodes_mutex);
		return NULL;
	}

	gcn->ref++;

	leave_mutex(&dinf->nodes_mutex);

	return gcn;
}
