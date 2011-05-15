#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>

char txt1[] = "MESSAGE!!";
char txt2[] = "\nMESSAGE SENT!!";
char txt3[] = "\nWaiting for Message...";
char txt4[] = "\nThis program must run as a service...";

char cmd[256];

unsigned char perms[0x1000] __attribute__ ((aligned (0x1000)));

void run(void) {
	struct csl_io_msg iomsg;
	int csl[4], msg[4];	
	int i;

    open_port(0, 0, UNRESTRICTED);
    open_port(1, 0, UNRESTRICTED);

    /* Let pman know we where started */
    send_msg(PMAN_TASK, 5, &csl);
	
	while (get_msg_count(0)==0);

    // get command line
	get_msg(0, csl, &i);

    read_mem(csl[1], 0, mem_size(csl[1]), cmd);

    if(cmd[7] == 'w')
    {
        // Send message to console
	    iomsg.command = CSL_WRITE;
	    iomsg.smo = share_mem(CONS_TASK, txt3, 24, READ_PERM);
	    iomsg.attribute = 0x7;
	    iomsg.response_code = 0;
	    send_msg(CONS_TASK, 1+csl[0], &iomsg);
        while(get_msg_count(0)==0){ reschedule();}
        get_msg(0, msg, &i);
        claim_mem(iomsg.smo);

        // set our thread permissions bitmap
        // NOTE: We cannot disable messages from privilege 0
        // tasks, so we will disable it for every other
        // task
        for(i = 0; i < 0x1000; i++)
        {
            perms[i] = 0xFF;
        }
        *((int*)perms) = 0x1000;
        *((int*)(perms+4)) = 0;

        set_port_mode(1,0,PERM_REQ);
        if(set_port_perm(1,(struct port_perms*)perms) != 0)
        {
            iomsg.command = CSL_WRITE;
	        iomsg.smo = share_mem(CONS_TASK, txt4, 39, READ_PERM);
	        iomsg.attribute = 0x7;
	        iomsg.response_code = 0;
	        send_msg(CONS_TASK, 1+csl[0], &iomsg);
            while(get_msg_count(0)==0){ reschedule();}
            get_msg(0, msg, &i);
            claim_mem(iomsg.smo);
        }
        else
        {   
            // wait for a message
            while(get_msg_count(1)==0) 
            {
                reschedule();
            }

            // Send message to console
            iomsg.command = CSL_WRITE;
            iomsg.smo = share_mem(CONS_TASK, txt1, 10, READ_PERM);
            iomsg.attribute = 0xC;
            iomsg.response_code = 0;
            send_msg(CONS_TASK, 1+csl[0], &iomsg);

            while(get_msg_count(0)==0){ reschedule();}
            get_msg(0, msg, &i);
            claim_mem(iomsg.smo);
            //////////////////////////
        }
    }
    else
    {
        // send message to the first process
        send_msg(16,1,csl);

        // Send message to console and invoke PMAN
	    iomsg.command = CSL_WRITE;
	    iomsg.smo = share_mem(CONS_TASK, txt2, 16, READ_PERM);
	    iomsg.attribute = 0xC;
	    iomsg.response_code = 0;
	    send_msg(CONS_TASK, 1+csl[0], &iomsg);
        while(get_msg_count(0)==0){ reschedule();}
        get_msg(0, msg, &i);
        claim_mem(iomsg.smo);
    }
	
	send_msg(PMAN_TASK, 4, csl);
	
	while(1) {reschedule();}
} 
