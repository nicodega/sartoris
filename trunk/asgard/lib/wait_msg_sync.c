/*
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

/*
This file provides blocking messaging waiting functions.
*/

#include <sartoris/syscall.h>
#include <os/pman_task.h>
#include <lib/scheduler.h>
#include <lib/wait_msg_sync.h>
#include <services/pmanager/services.h>

/*
This function will check for messages and if there are none, it will block the thread until there are.
*/
int wait_for_msg(int port)
{
    int ret = 0;
    struct pm_msg_block_thread msg;
    
    if(port > 31) return -1;

    while(!(ret = get_msg_count(port)))
    {
        msg.pm_type = PM_BLOCK_THREAD;
        msg.req_id = 0;
        msg.block_type = THR_BLOCK_MSG;
        msg.thread_id = get_current_thread();
        msg.port_mask = (0x1 << port);

        send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

        reschedule();
    }

    return ret;
}

int wait_for_msgs(int *ports, int *counts, int length)
{
    int ret = 0, i;
    unsigned int mask = 0;
    
    for(i = 0; i < length; i++)
    {
        if(ports[i] > 31) return -1;
        mask |= (0x1 << ports[i]);
    }

    return wait_for_msgs_masked(ports, counts, length, mask);
}

int wait_for_msgs_masked(int *ports, int *counts, int length, unsigned int mask)
{
    int ret = 0;
    struct pm_msg_block_thread msg;
    
    ret = get_msg_counts(ports, counts, length);

    while(!ret)
    {
        msg.pm_type = PM_BLOCK_THREAD;
        msg.req_id = 0;
        msg.block_type = THR_BLOCK_MSG;
        msg.thread_id = get_current_thread();
        msg.port_mask = mask;

        send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

        reschedule();

        ret = get_msg_counts(ports, counts, length);
    }
    
    return ret;
}
