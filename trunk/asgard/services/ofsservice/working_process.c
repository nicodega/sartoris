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

/* This function represents a working thread. */
#ifdef WIN32DEBUGGER
DWORD WINAPI working_process(LPVOID lpParameter)
#else
void working_process()
#endif
{
	int myid = -1;
	struct working_thread *thread = NULL;
	waiting_command *waiting_cmd = NULL;
	device_info *dinf = NULL;
	struct stdfss_res *ret = NULL;

#ifndef WIN32DEBUGGER
	__asm__ ("sti"::);
#endif

	// get thread id
	myid = new_threadid;

	thread = &working_threads[myid];
	
	set_wait_for_signal(myid, OFS_THREADSIGNAL_START, -1);

	thread->initialized = 1; // set as initialized

	while(1)
	{
		// wait until a START signal is rised 
		wait_for_signal(myid);

		thread->active = 1;

		// remember to get slots on evey process and free them when finished
		// using defined function here. Same goes for buffers.

		// start new command processing
		switch(thread->command.command)
		{
			case STDFSS_MOUNT:
				ret = mount_device(myid, thread, (struct stdfss_mount *)&thread->command);
				break;
			case STDFSS_UMOUNT:
				ret = umount_device(myid, thread, (struct stdfss_umount *)&thread->command);
				break;
			case STDFSS_OPEN:
				ret = open_file(myid, thread, (struct stdfss_open *)&thread->command);
				break;
			case STDFSS_CLOSE:
				ret = close_file(myid, thread, (struct stdfss_close *)&thread->command);
				break;
			case STDFSS_SEEK:
				ret = seek_file(myid, thread, (struct stdfss_seek *)&thread->command);
				break;
			case STDFSS_TELL:
				ret = tell_file(myid, thread, (struct stdfss_tell *)&thread->command);
				break;
			case STDFSS_READ:
				ret = read_file(myid, thread, (struct stdfss_read *)&thread->command, FALSE);
				break;
			case STDFSS_GETC:
				((struct stdfss_read *)&thread->command)->count = 1;
				ret = read_file(myid, thread, (struct stdfss_read *)&thread->command, FALSE);
				break;
			case STDFSS_GETS:
				ret = read_file(myid, thread, (struct stdfss_read *)&thread->command, TRUE);
				break;
			case STDFSS_PUTC:
				((struct stdfss_write *)&thread->command)->count = 1;
				ret = write_file(myid, thread, (struct stdfss_write *)&thread->command, FALSE);
				break;
			case STDFSS_PUTS:
				ret = write_file(myid, thread, (struct stdfss_write *)&thread->command, TRUE);
				break;
			case STDFSS_WRITE:
				ret = write_file(myid, thread, (struct stdfss_write *)&thread->command, FALSE);
				break;
			case STDFSS_IOCTL:
				ret = ioctl(myid, thread, (struct stdfss_ioctl *)&thread->command);
				break;
			case STDFSS_LINK:
				ret = link_file(myid, thread, (struct stdfss_link *)&thread->command);
				break;
			case STDFSS_DELETE:
				ret = delete_file(myid, thread, (struct stdfss_delete *)&thread->command);
				break;
			case STDFSS_FILEINFO:
				ret = file_info(myid, thread, (struct stdfss_fileinfo *)&thread->command);
				break;
			case STDFSS_MKDIR:
				ret = mkdir(myid, thread, (struct stdfss_mkdir *)&thread->command);
				break;
			case STDFSS_MKDEVICE:
				ret = mkdevice(myid, thread, (struct stdfss_mkdevice *)&thread->command);
				break;
			case STDFSS_CHANGEATT:
				ret = change_attributes(myid, thread, (struct stdfss_changeatt *)&thread->command);
				break;
			case STDFSS_FLUSH:
				ret = flush_buffers(myid, thread, (struct stdfss_flush *)&thread->command);
				break;
			case STDFSS_EXISTS:
				ret = exists(myid, thread, (struct stdfss_exists *)&thread->command);
				break;
			default:
				ret = build_response_msg(thread->command.command, STDFSSERR_INVALID_COMMAND);
				break;
		}

		// free thread resources
		/*if(thread->lastdir_parsed_node != NULL)
		{
			nfree(thread->lastdir_parsed_node);
			thread->lastdir_parsed_node = NULL;
		}*/
		thread->waiting_for_signal = 0;
		thread->signal_type = OFS_THREADSIGNAL_NOSIGNAL; // no signal is expected
		thread->directory_service_msg = -1;
		thread->locked_deviceid = -1;
		thread->locked_logic_deviceid = -1;
		thread->buffer_wait = 0;
		thread->locked_device = 0;
		thread->mounting = 0;
		working_threads[myid].locked_node = -1;
		working_threads[myid].locked_dirnode = -1;

		// remove command from processing list of device
		dinf = get_cache_device_info(thread->deviceid, thread->logic_deviceid);
		
		// check for NULL just in case it was a closed and dinf was removed
		if(dinf != NULL)
		{
			wait_mutex(&dinf->processing_mutex);
			{
				waiting_command *waiting_cmd = (waiting_command *)avl_getvalue(dinf->procesing, thread->taskid);
				avl_remove(&dinf->procesing, thread->taskid);
				free(waiting_cmd);
			}
			leave_mutex(&dinf->processing_mutex);
		}
		
		// exclude gets for it's a blocking char dev
		if(ret != NULL)
		{
			ret->thr_id = thread->command.thr_id;
			
			// send response
			if(send_msg(thread->taskid, thread->command.ret_port, ret))
			{
			}

			free(ret);
			ret = NULL;
		}
	
		// this should be removed, it's ment to check lockings
		if(own_mutex(&idle_threads_mutex)) print("idle_threads_mutex left locked", thread->command.command);
		if(own_mutex(&mounted_mutex)) print("mounted_mutex left locked", thread->command.command);
		if(own_mutex(&cached_devices_mutex)) print("cached_devices_mutex left locked", thread->command.command);
		if(own_mutex(&node_lock_mutex)) print("node_lock_mutex left locked", thread->command.command);
		if(own_mutex(&opened_files_mutex)) print("opened_files_mutex left locked", thread->command.command);
		if(own_mutex(&file_buffers_mutex)) print("file_buffers_mutex left locked", thread->command.command);
		if(own_mutex(&device_lock_mutex)) print("device_lock_mutex left locked", thread->command.command);
		if(own_mutex(&max_directory_msg_mutex)) print("max_directory_msg_mutex left locked", thread->command.command);

		set_wait_for_signal(myid, OFS_THREADSIGNAL_START, -1);
		
		thread->active = 0;
		signal_idle();
	}
	
}


     
