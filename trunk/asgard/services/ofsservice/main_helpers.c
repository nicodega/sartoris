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
#include <services/pmanager/services.h>

/* ofs internals */
int attempt_start(device_info *logic_device, CPOSITION position, int deviceid, int logic_deviceid)
{
	idle_device *idle_dev = NULL;
	CPOSITION it = NULL;
	struct swaiting_command *waiting_cmd = NULL;

	wait_mutex(&logic_device->processing_mutex);

	// see if the device is not processing commands or the
	// command can be issued concurrently with other commands 
	if(avl_get_total(&logic_device->procesing) == 0 || check_concurrent(logic_device, position))
	{
		// put the command in front of the waiting list //
		bring_to_front(&logic_device->waiting, position);
						
		// add the device to the processing_queue list, for processing //
		idle_dev = (idle_device *)malloc(sizeof(idle_device));

		idle_dev->logic_deviceid = logic_deviceid;
		idle_dev->deviceid = deviceid;

		add_tail(&processing_queue, idle_dev);

		// decrement all waiting commands lifetime
		it = get_head_position(&logic_device->waiting);

		while(it != NULL)
		{
			waiting_cmd = (struct swaiting_command *)get_next(&it);
			if(waiting_cmd->life_time != 0) waiting_cmd->life_time--;
		}
		leave_mutex(&logic_device->processing_mutex);

		return TRUE;
	}
	
	leave_mutex(&logic_device->processing_mutex);

	return FALSE;
}

int check_concurrent(struct sdevice_info *device, CPOSITION position)
{
	struct swaiting_command *waiting_cmd = NULL;
	CPOSITION it = NULL;

	waiting_cmd = (struct swaiting_command *)get_at(position);

	// UMOUNTS are NOT concurrent
	if(waiting_cmd->command.command == STDFSS_UMOUNT) return FALSE;

	// check the task is not processing something (one process running at the same time for a given task)
	if(avl_getvalue(device->procesing, waiting_cmd->sender_id) != NULL)
	{
		return FALSE;
	}

	// check there are no commands with 0 lifetime and no umount command is waiting
	it = get_head_position(&device->waiting);

	while(it != NULL)
	{
		waiting_cmd = (struct swaiting_command *)get_next(&it);
		if(waiting_cmd->life_time == 0 || waiting_cmd->command.command == STDFSS_UMOUNT) return FALSE;
	}

	// check there is no umount command being executed
	if(device->umount == TRUE) return FALSE;

	/* ok.. here we trust on how concurrency has been managed 
	   and let god judge us if a mutex or locking mecanism is wrong :\ 
	*/
	return TRUE;
}
int check_waiting_commands()
{
	int i = 0;
	AvlTree *sub_avl = NULL;
	device_info *dinf = NULL;
	int created = FALSE;
	
	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].initialized == 1 && working_threads[i].active == 0)
		{
			// check working thread last device for a new job
			wait_mutex(&cached_devices_mutex);
			sub_avl = (AvlTree *)avl_getvalue(cached_devices, working_threads[i].deviceid);

			if(sub_avl == NULL)
			{
				leave_mutex(&cached_devices_mutex);
				i++;
				continue;
			}

			dinf = (device_info *)avl_getvalue(*sub_avl, working_threads[i].logic_deviceid);

			if(dinf == NULL)
			{
				leave_mutex(&cached_devices_mutex);
				i++;
				continue;
			}
			leave_mutex(&cached_devices_mutex);

			if(length(&dinf->waiting) != 0)
			{
				// attempt starting this command
				if(attempt_start(dinf, get_head_position(&dinf->waiting), working_threads[i].deviceid, working_threads[i].logic_deviceid))
				{
					created = TRUE;
				}
			}
		}
		i++;
	}

	return created;
}





/* Modify this functions code to port to sartoris */
int create_working_thread(int id)
{
	struct pm_msg_create_thread msg_create_thr;
	struct pm_msg_response      msg_res;
	int sender_id;

	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = 0;
	msg_create_thr.response_port = OFS_PMAN_PORT;
	msg_create_thr.task_id = get_current_task();
	msg_create_thr.flags = 0;
	msg_create_thr.interrupt = 0;
	msg_create_thr.entry_point = &working_process;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	while (get_msg_count(OFS_PMAN_PORT) == 0) reschedule();

	get_msg(OFS_PMAN_PORT, &msg_res, &sender_id);

	if (msg_res.status != PM_THREAD_OK) 
	{
		return FALSE;
	}

	working_threads[id].threadid = msg_res.new_id;

	// wait until thread get's it's ID
	while(!working_threads[id].initialized){ reschedule(); }

	return TRUE;
}

int destroy_working_thread(int id)
{
	struct pm_msg_destroy_thread pm_destroy;
	struct pm_msg_response pm_res;
	int senderid;

	if(working_threads[id].active || !working_threads[id].initialized) return FALSE;

	pm_destroy.pm_type = PM_DESTROY_THREAD;
	pm_destroy.thread_id = working_threads[id].threadid;
	pm_destroy.response_port = OFS_PMAN_PORT;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &pm_destroy);

	while(get_msg_count(OFS_PMAN_PORT) == 0){ reschedule(); }

	get_msg(OFS_PMAN_PORT, &pm_res, &senderid);

	if(pm_res.status != PM_THREAD_OK) return FALSE;

	working_threads[id].active = 0;
	working_threads[id].initialized = 0;
	working_threads[id].threadid = -1;

	return TRUE;
}

int get_idle_working_thread()
{
	int id = 0;

	// first attempt to assign initialized threads
	while(id < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[id].active == 0 && working_threads[id].initialized) return id;
		id++;
	}

	id= 0;

	// assign any non initialized thread
	while(id < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[id].active == 0) return id;
		id++;
	}

	return -1;
}

void cleanup_working_threads()
{
	int idle_initialized_threads = 0, i = 0, j = 0;
	
	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].initialized == 1 && working_threads[i].active == 0) idle_initialized_threads++;
		i++;
	}

	if(idle_initialized_threads >= OFS_MAXINITILIZED_IDLE_WORKINGTHREADS)
	{
		// cleanup
		i =0;

		while(i < OFS_MAXWORKINGTHREADS && j < OFS_WOKINGTHREADS_CLEANUPAMMOUNT)
		{
			if(working_threads[i].initialized == 1 && working_threads[i].active == 0)
			{
				destroy_working_thread(i);
				j++;
			}
			i++;
		}
	}
}

int get_msg_working_process(int deviceid, int logic_deviceid, int msgid)
{
	int i = 0;

	// msgid will be the working thread id
	if(msgid >= OFS_MAXWORKINGTHREADS)
	{
		return -1;
	}

	if(working_threads[msgid].locked_device != 0)
	{
		if(working_threads[msgid].locked_deviceid == deviceid)
			return msgid;
	}
	else if(working_threads[msgid].mounting != 0)
	{
		if(working_threads[msgid].locked_logic_deviceid == logic_deviceid && working_threads[msgid].locked_deviceid == deviceid)
			return msgid;
	}
	else if(working_threads[msgid].buffer_wait != 0)
	{
		while(i < OFS_MAXWORKINGTHREADS)
		{
			if(working_threads[i].deviceid == deviceid && (working_threads[i].logic_deviceid == logic_deviceid || logic_deviceid == -1)) return msgid;
			i++;
		}
	}
	else if(working_threads[i].buffer_wait == 0)
	{
		while(i < OFS_MAXWORKINGTHREADS)
		{
			if(working_threads[i].deviceid == deviceid && (working_threads[i].logic_deviceid == logic_deviceid || logic_deviceid == -1)) return msgid;
			i++;
		}
	}

	return -1;
}

/* helpers */
struct stdfss_res *build_response_msg(unsigned int command, unsigned int ret)
{
	struct stdfss_res *res_msg = (struct stdfss_res *)malloc(sizeof(struct stdfss_res));

	res_msg->command = command;
	res_msg->ret = ret;

	return res_msg;
}

int bitmaps_size(int node_count, int block_count)
{
	int nodes = ((int)(node_count / BITS_PER_DEVBLOCK) + ((node_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	int blocks = ((int)(block_count / BITS_PER_DEVBLOCK) + ((block_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	return nodes * OFS_BLOCKDEV_BLOCKSIZE + blocks * OFS_BLOCKDEV_BLOCKSIZE;
}

void mem_copy(unsigned char *source, unsigned char *dest, int count)
{
	int i = 0;
	
	while(i < count){
		dest[i] = source[i];
		i++;
	}
}

char *get_string(int smo)
{
	//get_string: gets a string from a Shared Memory Object
	int size = mem_size(smo);

	if(size == 0) return NULL;

	char *tmp = (char *)malloc(size);

	if(read_mem(smo, 0, size, tmp))
	{
		return NULL;		
	}

	size--;
	while(tmp[size] != '\0' && size >= 0){size--;}
	
	if(size == -1)
	{
		return NULL;
	}

	return tmp;
}


short get_file_id(struct stask_info * tinf, struct stask_file_info *finf)
{
	// returns a free file id for the given fileinfo on the task
	int *ids = NULL;
	int count = 0, i = 0;
	short id = 0;

	count = avl_get_indexes(&tinf->open_files, &ids);

	i = 0;
	// get a free number on the array
	// NOTE: avl_get_indexes returns ids ordered from lesser to grater
	while(i < count)
	{
		if(id != ids[i]) break; // found a free id

		id = ids[i] + 1; // set id to the next number available so far
		i++;
	}

	// free ids array
	if(ids != NULL) free(ids);

	return id;
}

/* init helpers */
void init_working_threads()
{
	int i = 0, j = 0;

	while(i < OFS_MAXWORKINGTHREADS)
	{
		working_threads[i].active = 0;
		working_threads[i].threadid = -1;
		working_threads[i].waiting_for_signal = 0;
		working_threads[i].signal_type = OFS_THREADSIGNAL_NOSIGNAL; // no signal is expected
		working_threads[i].initialized = 0;
		working_threads[i].directory_service_msg = -1;
		working_threads[i].locked_deviceid = -1;
		working_threads[i].locked_logic_deviceid = -1;
		working_threads[i].buffer_wait = 0;
		working_threads[i].locked_device = 0;
		working_threads[i].mounting = 0;

		init_mutex(&working_threads[i].waiting_for_signal_mutex);

		j = 0;
		while(j < OFS_MAXWORKINGTHREADS)
		{
			working_threads[j].processes_waiting_lockeddevice[j] = 0; // process is not waiting
			j++;
		}

		i++;
	}

}



void free_mountinfo_struct(struct smount_info *minf, int wpid)
{
	// free resources used by mount info structure	
	struct stddev_freedev_cmd freedevice_cmd;
	struct stddev_res response;
	int i = 0, sender;

	freedevice_cmd.command = STDDEV_FREE_DEVICE;
	freedevice_cmd.ret_port = OFS_STDDEVRES_PORT;
	freedevice_cmd.msg_id = wpid;
	freedevice_cmd.logic_deviceid = minf->logic_deviceid;

	while(i != minf->ofst.group_count)
	{
		if(minf->group_m_free_blocks[i].bits != NULL)
		{
			free(minf->group_m_free_blocks[i].bits);
		}
		if(minf->group_m_free_nodes[i].bits != NULL)
		{
			free(minf->group_m_free_nodes[i].bits);
		}
		i++;
	}
	free(minf->group_m_free_blocks);
	free(minf->group_m_free_nodes);

	minf->group_m_free_blocks = NULL;
	minf->group_m_free_nodes = NULL;

	// free the device
	if(wpid == -1)
	{
		send_msg(minf->deviceid, minf->dinf->stddev_port, &freedevice_cmd);

		while(get_msg_count(OFS_STDDEVRES_PORT) == 0){reschedule();}

		get_msg(OFS_STDDEVRES_PORT, &response, &sender);
	}
	else
	{
		locking_send_msg(minf->deviceid, minf->dinf->stddev_port, &freedevice_cmd, wpid);
	}
	
	free_device_struct(minf->dinf);
	minf->dinf = NULL;

	free(minf->group_headers);

	minf->group_headers = NULL;

	close_mutex(&minf->free_space_mutex);
}

void clear_dir_buffer(unsigned char *buffer)
{
	int i = 0, size = OFS_DIR_BUFFERSIZE / 4, *ptr;

	ptr = (int *)buffer;

	while(i < size)
	{
		ptr[i] = 0;
		i++;
	}
}



