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

// sends a message with OFS_THREADSIGNAL_MSG signaling
int locking_send_msg(int totask, int port, void *msg, int wpid)
{
	set_wait_for_signal(wpid, OFS_THREADSIGNAL_MSG, totask);
	
	if(send_msg(totask, port, msg))
	{
		return FALSE;
	}

	// wait for signal
	wait_for_signal(wpid);

	return TRUE;
}

// gets the signal response message
void get_signal_msg(int *dest, int wpid)
{
	dest[0] = working_threads[wpid].signal_msg[0];
	dest[1] = working_threads[wpid].signal_msg[1];
	dest[2] = working_threads[wpid].signal_msg[2];
	dest[3] = working_threads[wpid].signal_msg[3];
}

void signal_idle()
{
    wait_mutex(&idle_threads_mutex);
    idle_threads++;
    leave_mutex(&idle_threads_mutex);
}

void decrement_idle()
{
	wait_mutex(&idle_threads_mutex);
	idle_threads--;
	leave_mutex(&idle_threads_mutex);
}
int check_idle()
{
	int ret;
	wait_mutex(&idle_threads_mutex);
	ret = idle_threads > 0;	
	leave_mutex(&idle_threads_mutex);
	return ret;
}

// this function should be invoked before sending msgs
void set_wait_for_signal(int threadid, int signal_type, int senderid)
{
	wait_mutex(&working_threads[threadid].waiting_for_signal_mutex);
	working_threads[threadid].expected_signal_type = signal_type;
	working_threads[threadid].signal_senderid = senderid;
	working_threads[threadid].waiting_for_signal = 1;
	leave_mutex(&working_threads[threadid].waiting_for_signal_mutex);
}

void wait_for_signal(int threadid)
{
    int msg[4];

    if(working_threads[threadid].expected_signal_type == OFS_THREADSIGNAL_START && working_threads[threadid].initialized)
    {
        msg[0] = threadid;
         __asm__ __volatile__ ("cli"::);
        send_msg(get_current_task(), OFS_IDLE_PORT, &msg);
        working_threads[threadid].active = 0;
         __asm__ __volatile__ ("sti"::);
    }

    while(working_threads[threadid].waiting_for_signal == 1)
    { 
        reschedule(); // thread will block here or just after the send msg...
    }
}

void wp_sleep(int threadid)
{
    struct pm_msg_block_thread msg;

    msg.pm_type = PM_BLOCK_THREAD;
    msg.req_id = 0;
    msg.block_type = THR_BLOCK;
    msg.thread_id = working_threads[threadid].threadid;

    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);
}

void wake_wp(int threadid)
{   
	// signal thread for start
	signal(threadid, NULL, -1, OFS_THREADSIGNAL_START);

    struct pm_msg_unblock_thread unb_msg;

    unb_msg.pm_type = PM_UNBLOCK_THREAD;
    unb_msg.req_id = 0;
    unb_msg.thread_id = working_threads[threadid].threadid;
    unb_msg.response_port = OFS_PMAN_PORT;

    // we must ensure active is set before the thread can finish processing the command
    __asm__ __volatile__ ("cli"::);
    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &unb_msg);
    
    // set the thread to active so we don't select it again
    working_threads[threadid].active = 1;

    __asm__ __volatile__ ("sti"::);
}

void signal(int threadid, int *msg, int senderid, int signal_type)
{
	int i = 0;

	wait_mutex(&working_threads[threadid].waiting_for_signal_mutex);
	if(working_threads[threadid].signal_senderid != senderid || working_threads[threadid].waiting_for_signal == 0 || working_threads[threadid].expected_signal_type != signal_type)
	{
		// unexpected signal, discard it
		leave_mutex(&working_threads[threadid].waiting_for_signal_mutex);
		return;
	}

	// send signal
	working_threads[threadid].expected_signal_type = OFS_THREADSIGNAL_NOSIGNAL;
	working_threads[threadid].signal_type = signal_type;
	working_threads[threadid].signal_senderid = senderid;

	if(msg != NULL)
	{
		working_threads[threadid].signal_msg[0] = *msg++;
		working_threads[threadid].signal_msg[1] = *(msg++);
		working_threads[threadid].signal_msg[2] = *(msg++);
		working_threads[threadid].signal_msg[3] = *(msg);
	}
        
	leave_mutex(&working_threads[threadid].waiting_for_signal_mutex);

	working_threads[threadid].waiting_for_signal = 0;
}

int get_resolution_signal_wp()
{
	int i = 0;

	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].directory_service_msg == 0)
			return i;
		i++;
	}

	return -1;
}
