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

int lock_node(int nodeid, int lock_mode, int failed_status, int wpid, int command, struct stdfss_res **ret)
{
	int i = 0, exclusive, waiting_locked, waiting_nodeid;
	CPOSITION it;
	struct node_lock_waiting *waiting_node = NULL;

	wait_mutex(&node_lock_mutex);

	// see if anyone has it on exclusive mode
	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].deviceid == working_threads[wpid].deviceid && working_threads[i].logic_deviceid == working_threads[wpid].logic_deviceid && ((working_threads[i].locked_node == nodeid && (working_threads[i].locked_node_exclusive || (lock_mode & OFS_NODELOCK_EXCLUSIVE))) || (working_threads[wpid].locked_dirnode == nodeid  && (working_threads[i].locked_dirnode_exclusive  || (lock_mode & OFS_NODELOCK_EXCLUSIVE)))))
		{
			if(i == wpid){ break; }	
			
			// if non blocking --> error
			if((lock_mode & OFS_NODELOCK_BLOCKING) == 0)
			{
				*ret = build_response_msg(command, STDFSSERR_FILE_INUSE);
				return FALSE;
			}

			// if blocking, check we can wait safely without a deadlock
			
			waiting_nodeid = 0;
			waiting_locked = 0;

			if((working_threads[wpid].locked_dirnode != -1 && working_threads[wpid].locked_dirnode_exclusive) || (working_threads[wpid].locked_node != -1 && working_threads[wpid].locked_node_exclusive))
			{
				// someone waiting for my locked node and I have it on X mode?
				// someone waiting for nodeid on X mode?

				it = get_head_position(&lock_node_waiting);

				while(it != NULL)
				{
					waiting_node = (struct node_lock_waiting *)get_next(&it);

					exclusive = (waiting_node->lock_mode & OFS_NODELOCK_EXCLUSIVE) == OFS_NODELOCK_EXCLUSIVE;
					
					if(exclusive && waiting_node->nodeid == nodeid)
					{
						// someone waiting
						waiting_nodeid = 1;
					}
					if((working_threads[wpid].locked_node != -1 && working_threads[wpid].locked_node_exclusive && waiting_node->nodeid == working_threads[wpid].locked_node))
					{
						// someone waiting
						waiting_locked = 1;
					}
					if((working_threads[wpid].locked_dirnode != -1 && working_threads[wpid].locked_dirnode_exclusive && waiting_node->nodeid == working_threads[wpid].locked_dirnode))
					{
						// someone waiting
						waiting_locked = 1;
					}
				}
			}

			if( waiting_nodeid && waiting_locked )
			{
				// we would enter a deadlock here, so forget it
				*ret = build_response_msg(command, STDFSSERR_FILE_INUSE);
				return FALSE;
			}
			
			// no deadlock can happen (lets hope the algorithm is ok)
			set_wait_for_signal(wpid, OFS_THREADSIGNAL_NODELOCK, -1);

			waiting_node = (struct node_lock_waiting *)malloc(sizeof(struct node_lock_waiting));

			waiting_node->wpid = wpid;
			waiting_node->lock_mode = lock_mode;
			waiting_node->nodeid = nodeid;
			waiting_node->failed_status = failed_status;

			add_tail(&lock_node_waiting, waiting_node);
			
			leave_mutex(&node_lock_mutex);

			// wait until node is granted
			wait_for_signal(wpid);

			if(working_threads[wpid].node_lock_signal_response != OFS_LOCKSTATUS_OK && (working_threads[wpid].node_lock_signal_response == failed_status || working_threads[wpid].node_lock_signal_response == OFS_LOCKSTATUS_DENYALL))
			{
				return FALSE;
			}
			else
			{
				return TRUE;
			}
			
		}
		i++;
	}

	// nobody has the node, lock it
	if(lock_mode & OFS_NODELOCK_DIRECTORY)
	{
		working_threads[wpid].locked_dirnode = nodeid;
		if(lock_mode & OFS_NODELOCK_EXCLUSIVE)
		{
			working_threads[wpid].locked_dirnode_exclusive = 1;
		}
	}
	else
	{
		working_threads[wpid].locked_node = nodeid;
		if(lock_mode & OFS_NODELOCK_EXCLUSIVE)
		{
			working_threads[wpid].locked_node_exclusive = 1;
		}
	}
	
	working_threads[wpid].node_lock_signal_response = 0;

	leave_mutex(&node_lock_mutex);

	return TRUE;
}

void unlock_node(int wpid, int directory, int status)
{
	int nodeid, i, exclusive;
	CPOSITION it, oldit;
	struct node_lock_waiting *waiting_node = NULL;
	wait_mutex(&node_lock_mutex);
	
    if(directory)
	{
        if(working_threads[wpid].locked_dirnode == -1)
        {
            leave_mutex(&node_lock_mutex);
            return;
        }
	    nodeid = working_threads[wpid].locked_dirnode;
		working_threads[wpid].locked_dirnode = -1;
		working_threads[wpid].locked_dirnode_exclusive = 0;
	}
	else
	{
        if(working_threads[wpid].locked_node == -1)
        {
            leave_mutex(&node_lock_mutex);
            return;
        }
		nodeid = working_threads[wpid].locked_node;
		working_threads[wpid].locked_node = -1;
		working_threads[wpid].locked_node_exclusive = 0;
	}
	
	it = get_head_position(&lock_node_waiting);

	// see if someone is waiting for this node
	while(it != NULL)
	{
		oldit = it;
		waiting_node = (struct node_lock_waiting *)get_next(&it);

		if(waiting_node->nodeid == nodeid)
		{
			if(status == waiting_node->failed_status || status == OFS_LOCKSTATUS_DENYALL)
			{
				// sorry, you cant have it
				working_threads[waiting_node->wpid].node_lock_signal_response = status;
				
				remove_at(&lock_node_waiting, oldit);
				free(waiting_node);

				signal(waiting_node->wpid, NULL, -1, OFS_THREADSIGNAL_NODELOCK);
				continue;
			}
			
			exclusive = ((waiting_node->lock_mode & OFS_NODELOCK_EXCLUSIVE)? 1 : 0);

			// se if anyone has it
			i = 0;

			while(i < OFS_MAXWORKINGTHREADS)
			{
				if(working_threads[i].deviceid == working_threads[waiting_node->wpid].deviceid 
                    && working_threads[i].logic_deviceid == working_threads[waiting_node->wpid].logic_deviceid 
                    && ((working_threads[i].locked_node == nodeid 
                    && (working_threads[i].locked_node_exclusive 
                        || (waiting_node->lock_mode & OFS_NODELOCK_EXCLUSIVE))) 
                        || (working_threads[waiting_node->wpid].locked_dirnode == nodeid  && (working_threads[i].locked_dirnode_exclusive 
                        || (waiting_node->lock_mode & OFS_NODELOCK_EXCLUSIVE)))))
				{
					// ok everybody just keep on waiting
					break;
				}
				i++;
			}
			
			// it's ok to lock the node...
			if(waiting_node->lock_mode & OFS_NODELOCK_DIRECTORY)
			{
				working_threads[waiting_node->wpid].locked_dirnode = nodeid;
				working_threads[i].locked_dirnode_exclusive = ((exclusive)? 1 : 0);
			}
			else
			{
				working_threads[waiting_node->wpid].locked_node = nodeid;
				working_threads[i].locked_node_exclusive = ((exclusive)? 1 : 0);
			}
			
			working_threads[waiting_node->wpid].node_lock_signal_response = status;						

			signal(waiting_node->wpid, NULL, -1, OFS_THREADSIGNAL_NODELOCK);

			remove_at(&lock_node_waiting, oldit);
			free(waiting_node);

			// if not exclusive, keep on looking for non-exclusive
			if(exclusive) break;
		}
	}
	
	leave_mutex(&node_lock_mutex);
}
