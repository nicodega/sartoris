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


// Global Variables declaration //

AvlTree tasks;
struct mutex opened_files_mutex;		
list opened_files;					

list processing_queue;		
struct mutex pending_devices_mutex;
lpat_tree mounted;		
struct mutex mounted_mutex;

AvlTree cached_devices;	
int cached_devices_count;
struct mutex cached_devices_mutex;

char mbuffer[1024 * 1024]; 
char working_processes_staks[OFS_MAXWORKINGTHREADS][1024 * 5]; 

struct file_buffer file_buffers[OFS_FILEBUFFERS];
struct mutex file_buffers_mutex;

struct working_thread working_threads[OFS_MAXWORKINGTHREADS];
int new_threadid;	

int idle_threads;
struct mutex idle_threads_mutex;
	
int initialized;

list lock_node_waiting;
struct mutex node_lock_mutex;
struct mutex device_lock_mutex;
	
int max_directory_msg;
struct mutex max_directory_msg_mutex;

/* shutdown ok message */
struct stdservice_res dieres;
int dieid;
int dieretport;

// This is the main service thread //
void _start()
{
	int die = 0, blocked = 0, senderid, idle_thread, idle_devs, wpid, value, i;
	int chardev_count, blockdev_count, stdfss_count, service_count, stddevres_count, dirservice_count; // counters for msg on ports preventing DOS 
	int deviceid, logic_deviceid;
	device_info *dinf;
	idle_device *idle_dev;
	struct stdchardev_res incoming_char;
	struct stdblockdev_res incoming_block;
	struct stdservice_cmd service_cmd;
	struct stdfss_cmd cmd;
	struct stdfss_res res, *ret;
	struct stdfss_ver_res ver_res;
	struct stddev_res stddev_res;
	struct directory_response dirservice_res;
	char *service_name = "fs/default";
	waiting_command *waiting_cmd;
	CPOSITION it = NULL;
	struct directory_register reg_cmd;
	struct directory_response dir_res;
	struct stdservice_res servres;
	struct stdservice_res dieres;

	/* open ports used */
	open_port(OFS_DIRECTORY_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(OFS_CHARDEV_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(OFS_BLOCKDEV_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(STDFSS_PORT, -1, UNRESTRICTED);
	open_port(STDSERVICE_PORT, 1, PRIV_LEVEL_ONLY); 		// should set perms only to the scheduler
	open_port(OFS_STDDEVRES_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(OFS_PMAN_PORT, 0, PRIV_LEVEL_ONLY);

	// set interrupts
	__asm__ ("sti"::);

	init_mem(mbuffer, 1024 * 1024);

	ver_res.command = STDFSS_VER;
	ver_res.ver = STDFSS_VER;
	ver_res.ret = STDFSSERR_OK;

	idle_threads = OFS_MAXWORKINGTHREADS; // all threads are idle
	init_working_threads();

	// register service //
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = OFS_DIRECTORY_PORT;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 11, READ_PERM);

	while (send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd) < 0)
    { 
        reschedule(); 
    }

	while (get_msg_count(OFS_DIRECTORY_PORT) == 0) { reschedule(); }

	get_msg(OFS_DIRECTORY_PORT, &dir_res, &senderid);

	claim_mem(reg_cmd.service_name_smo);
	//////////////////////

	/* init mutexes */ 
	init_mutex(&idle_threads_mutex);
	init_mutex(&mounted_mutex);
	init_mutex(&cached_devices_mutex);
	init_mutex(&node_lock_mutex);
	init_mutex(&opened_files_mutex);
	init_mutex(&file_buffers_mutex);
	init_mutex(&device_lock_mutex);
	init_mutex(&max_directory_msg_mutex);

	init(&lock_node_waiting);
	init_file_buffers();

	initialized = 0;
	cached_devices_count = 0;
	max_directory_msg = 0;

	/* init globals */
	init(&processing_queue);
	init(&opened_files);
	lpt_init(&mounted);
	avl_init(&tasks);
    
#ifdef OFS_DEVBLOCK_CACHE
	bc_init();
#endif
	while(!die)
	{
		while(get_msg_count(OFS_DIRECTORY_PORT) == 0 
			&& get_msg_count(OFS_CHARDEV_PORT) == 0 
			&& get_msg_count(OFS_BLOCKDEV_PORT) == 0 
			&& get_msg_count(STDFSS_PORT) == 0 
			&& get_msg_count(STDSERVICE_PORT) == 0 
			&& get_msg_count(OFS_STDDEVRES_PORT) == 0 
			&& (!check_idle() || (length(&processing_queue) == 0 && check_waiting_commands() == 0) )
		){ 
            string_print("OFS ALIVE",4*160 - 18,i++);
			reschedule(); 
		}

		dirservice_count = get_msg_count(OFS_DIRECTORY_PORT);

		// process incoming directory resolve responses
		while(dirservice_count != 0)
		{
			get_msg(OFS_DIRECTORY_PORT, &dirservice_res, &senderid);

			wpid = get_resolution_signal_wp();

			if(wpid == -1)
			{
				dirservice_count--;
				continue;
			}

			// signal thread
			signal(wpid, (int *)&dirservice_res, -1, OFS_THREADSIGNAL_DIRECTORYMSG);

			dirservice_count--;
		}


		service_count = get_msg_count(STDSERVICE_PORT);

		// process any STDSERVICE Mesages
		while(service_count != 0)
		{
			get_msg(STDSERVICE_PORT, &service_cmd, &senderid);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				blocked = 1; // block new commands processing for service shutdown
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				dieid = senderid;
				dieretport = service_cmd.ret_port;
			}
			else if(service_cmd.command == STDSERVICE_FORCEDIE)
			{
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				dieid = senderid;
				dieretport = service_cmd.ret_port;
				die = 1;
			}
			else if(service_cmd.command == STDSERVICE_PREPAREFORSLEEP)
			{
				// wait for the wake up message
				// NOTE: we rely on devices services to finish their jobs 
				blocked = 1;
				send_msg(senderid, service_cmd.ret_port, &servres);
				while(blocked)
				{
					while(get_msg_count(STDSERVICE_PORT) == 0);

					get_msg(STDSERVICE_PORT, &service_cmd, &senderid);

					if(service_cmd.command == STDSERVICE_WAKEUP)
					{
						// ok back in business
						blocked = 0;
						servres.ret = STDSERVICE_RESPONSE_OK;
						servres.command = service_cmd.command;
						send_msg(senderid, service_cmd.ret_port, &servres);
					}
					else
					{
						servres.ret = STDSERVICE_RESPONSE_FAILED;
						servres.command = service_cmd.command;
						send_msg(senderid, service_cmd.ret_port, &servres);
					}
				}
			}
			else
			{
				send_msg(senderid, service_cmd.ret_port, &servres);
			}
			service_count--;
		}

		stddevres_count = get_msg_count(OFS_STDDEVRES_PORT);
		
		// process working thread MSG signals
		while(stddevres_count != 0)
		{
			get_msg(OFS_STDDEVRES_PORT, &stddev_res, &senderid);

			wpid = get_msg_working_process(senderid, -1, stddev_res.msg_id);

			if(wpid == -1)
			{
				stddevres_count--;
				continue;
			}

			// signal thread
			signal(wpid, (int *)&stddev_res, senderid, OFS_THREADSIGNAL_MSG);

			stddevres_count--;
		}

		chardev_count = get_msg_count(OFS_CHARDEV_PORT);
		
		while(chardev_count != 0)
		{
			get_msg(OFS_CHARDEV_PORT, &incoming_char, &senderid);

			if(!check_blocked_device((struct stdchardev_res *)&incoming_char, senderid))
			{

				wpid = get_msg_working_process(senderid, incoming_char.dev, incoming_char.msg_id);

				if(wpid == -1)
				{
					chardev_count--;
					continue;
				}

				// signal thread
				signal(wpid, (int *)&incoming_char, senderid, OFS_THREADSIGNAL_MSG);
			}

			chardev_count--;
		}
		
		blockdev_count = get_msg_count(OFS_BLOCKDEV_PORT);
		
		while(blockdev_count != 0)
		{
			if(get_msg(OFS_BLOCKDEV_PORT, &incoming_block, &senderid) == SUCCESS)
			{
				wpid = get_msg_working_process(senderid, incoming_block.dev, incoming_block.msg_id);

				if(wpid == -1)
				{
					blockdev_count--;
					continue;
				}

				// signal thread
				signal(wpid, (int *)&incoming_block, senderid, OFS_THREADSIGNAL_MSG);

				blockdev_count--;
			}			
		}
		
		stdfss_count = get_msg_count(STDFSS_PORT);
		
		// if shuting down and al threads are inactive then die
		if(blocked)
		{
			stdfss_count = 0; // discard all messages for we are only waiting for shutdown
			i = 0;
			while(i < OFS_MAXWORKINGTHREADS)
			{
				if(working_threads[i].active == 1)
				{
					break;
				}
				i++;
			}
			if(i == OFS_MAXWORKINGTHREADS)
			{
				// ready to die
				die = 1;
			}
		}

		// process incoming STDFSS messages
		while(!die && stdfss_count != 0)
		{
			get_msg(STDFSS_PORT, &cmd, &senderid);		

			if(blocked)
			{
				// service is shuting down, send error msg
				ret = build_response_msg(cmd.command, STDFSSERR_STDFS_SERVICE_SHUTDOWN);
				ret->thr_id = cmd.thr_id;
				send_msg(senderid, cmd.ret_port, ret);
				free(ret);
				stdfss_count--;
			}
			else if(cmd.command == STDFSS_VERSION)
			{			
				ver_res.thr_id = cmd.thr_id;
				// send the version information //
				send_msg(senderid, cmd.ret_port, &ver_res);

				stdfss_count--;

				continue;
			}
			else if (cmd.command == STDFSS_INIT)
			{
				ret = NULL;
				init_ofs((struct stdfss_init *)&cmd, &ret);

				if(ret != NULL)
				{
					ret->thr_id = cmd.thr_id;
					send_msg(senderid, ((struct stdfss_init *)&cmd)->ret_port, ret);
					free(ret);
					ret = NULL;
				}

				stdfss_count--;
			}
			else
			{
				if(!initialized)
				{
					// send not initialized
					ret = build_response_msg(cmd.command, STDFSSERR_SERVICE_NOT_INITIALIZED);
					ret->thr_id = cmd.thr_id;
					send_msg(senderid, ((struct stdfss_init *)&cmd)->ret_port, ret);
					free(ret);
					ret = NULL;
					stdfss_count--;
					continue; 
				}

				// get service and logic device id (and process forward/return/takeover) //
				switch(cmd.command)
				{
					case STDFSS_FORWARD:
						/* forwarding won't be supported by this service */
						/* for it will serve as the default fs */
						res.command = cmd.command;
						res.thr_id = cmd.thr_id;
						res.ret = STDFSSERR_COMMAND_NOTSUPPORTED;

						send_msg(senderid, cmd.ret_port, &res);
						stdfss_count--;
						continue;
                    case STDFSS_RETURN:
                        if(senderid != PMAN_TASK)
                        {
                            ret = build_response_msg(cmd.command, STDFSSERR_NO_PRIVILEGES);
					        ret->thr_id = cmd.thr_id;
					        send_msg(senderid, ((struct stdfss_return*)&cmd)->ret_port, ret);
					        free(ret);
					        ret = NULL;
					        stdfss_count--;
					        continue; 
                        }

                        // NOTE: Just as on takeover, we will let all pending commands
                        // finish.
                        ret = begin_return(senderid, (struct stdfss_return*)&cmd, &deviceid, &logic_deviceid, &dinf);

                        if(ret != NULL)
                        {
                            ret->thr_id = cmd.thr_id;
					        send_msg(senderid, ((struct stdfss_return*)&cmd)->ret_port, ret);
					        free(ret);
					        ret = NULL;
					        stdfss_count--;
					        continue;
                        }
                        value = 1;
                        break;
                    case STDFSS_TAKEOVER:
                        if(senderid != PMAN_TASK)
                        {
                            ret = build_response_msg(cmd.command, STDFSSERR_NO_PRIVILEGES);
					        ret->thr_id = cmd.thr_id;
					        send_msg(senderid, ((struct stdfss_takeover*)&cmd)->ret_port, ret);
					        free(ret);
					        ret = NULL;
					        stdfss_count--;
					        continue; 
                        }

                        // NOTE: When a file is taken over, we won't allow other 
                        // commands from the original owner, but there might be
                        // pending commands on the queue.
                        // We will add the file to the taking over task open_files tree
                        // and set takeover properly. On get_device_info, for file commands 
                        // (read/write,get[s],put[s], flush, tell and close) we will check
                        // for takeover and fail if it's set to something other than NULL.
                        // since we want all pending operations by the current owner to
                        // finish, we will queue the command and execute it once all device 
                        // operations before takeover finish (i.e, takeover is not concurrent).
                        ret = begin_takeover(senderid, (struct stdfss_takeover*)&cmd, &deviceid, &logic_deviceid, &dinf);

                        if(ret != NULL)
                        {
                            ret->thr_id = cmd.thr_id;
					        send_msg(senderid, ((struct stdfss_takeover*)&cmd)->ret_port, ret);
					        free(ret);
					        ret = NULL;
					        stdfss_count--;
					        continue;
                        }
                        value = 1;
                        break;
					case STDFSS_CLOSE:
					case STDFSS_SEEK:
					case STDFSS_READ:
					case STDFSS_WRITE:
					case STDFSS_FLUSH:
					case STDFSS_IOCTL:
					case STDFSS_TELL:
					case STDFSS_PUTC:
					case STDFSS_PUTS:
					case STDFSS_GETC:
					case STDFSS_GETS:
						value = get_device_info(&cmd, senderid, ((struct stdfss_fileid *)&cmd)->file_id, &deviceid, &logic_deviceid, &dinf, &ret);
					break;
					default:
						value = get_device_info(&cmd, -1, -1, &deviceid, &logic_deviceid, &dinf, &ret);
					break;
				}

				if(!value)
				{
					if(ret != NULL)
					{
						ret->thr_id = cmd.thr_id;
						
						send_msg(senderid, cmd.ret_port, ret);

						free(ret);
						ret = NULL;
					}
					else
					{
						// send an error message //
						res.command = cmd.command;
						res.thr_id = cmd.thr_id;
						res.ret = STDFSSERR_DEVICE_NOT_MOUNTED;

						send_msg(senderid, cmd.ret_port, &res);
					}
					stdfss_count--;
					continue;
				}
				else
				{
					// insert the request in the waiting list //
					waiting_cmd = (waiting_command *)malloc(sizeof(waiting_command));
					
					waiting_cmd->command = cmd;
					waiting_cmd->sender_id = senderid;
					waiting_cmd->life_time = OFS_COMMAND_LIFETIME;

					wait_mutex(&dinf->processing_mutex);
					add_tail(&dinf->waiting, (void *)waiting_cmd);
					leave_mutex(&dinf->processing_mutex);

					// cache device info again, just in case it has been removed
					dinf = cache_device_info(deviceid, logic_deviceid, dinf);

					dinf->hits++; // hit logic device

					if(!attempt_start(dinf, get_tail_position(&dinf->waiting), deviceid, logic_deviceid))
					{
						//print("failed to start",0);
					}

					stdfss_count--;
				}
			}
		}
	
		// start any idle commands if there are threads in idle state
		idle_devs = length(&processing_queue);

		while(!die && check_idle() && idle_devs > 0)
		{
			// there are idle threads wich can be started
			idle_thread = get_idle_working_thread();

			// fill working_thread structure
			idle_dev = (idle_device *)get_head(&processing_queue);

			dinf = get_cache_device_info(idle_dev->deviceid, idle_dev->logic_deviceid);

			waiting_cmd = (waiting_command *)get_head(&dinf->waiting);
		
			working_threads[idle_thread].command = waiting_cmd->command;
			working_threads[idle_thread].deviceid = idle_dev->deviceid;
			working_threads[idle_thread].logic_deviceid = idle_dev->logic_deviceid;
			working_threads[idle_thread].taskid = waiting_cmd->sender_id;
			working_threads[idle_thread].locked_device = 0;
			working_threads[idle_thread].buffer_wait = 0;
			working_threads[idle_thread].mounting = 0;
			working_threads[idle_thread].locked_node = -1;
			working_threads[idle_thread].locked_dirnode = -1;

			// create thread if it was not initialized
			if(working_threads[idle_thread].initialized == 0)
			{
				if(!create_working_thread(idle_thread))
				{
					// no new threads can be created
					idle_devs--;
					continue;
				}
			}

			decrement_idle();

			wait_mutex(&dinf->umount_mutex);
			if(waiting_cmd->command.command == STDFSS_UMOUNT)
			{
				dinf->umount = TRUE; // set umount flag to 1 so no other commands are started until umount finishes
			}
			leave_mutex(&dinf->umount_mutex);

			// add waiting command to procesing avl
			wait_mutex(&dinf->processing_mutex);
			{
				avl_insert(&dinf->procesing, waiting_cmd, waiting_cmd->sender_id);
				remove_at(&dinf->waiting, get_head_position(&dinf->waiting));
			}
			leave_mutex(&dinf->processing_mutex);

			// signal thread for start
			signal(idle_thread, NULL, -1, OFS_THREADSIGNAL_START);

			// when a thread is signaled for starting we should wait until it
			// becomes active
			while(working_threads[idle_thread].active = 0){ reschedule(); }

			// free structures used
			remove_at(&processing_queue, get_head_position(&processing_queue));
			
			free(idle_dev);

			idle_devs--;
		}

		// destroy threads if too many are idle
		cleanup_working_threads();
	}
	
	shutdown();
	
	// send die response
	send_msg(dieid, dieretport, &dieres);
}
