
/*
This program will test ttrace on the microkernel.
*/

#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>
#include <services/p-manager/pman.h>
#include <lib/printf.h>
#include <sartoris-i386/ttrace.h>

char cmd[256];
int console; // required by printf

void run(void) {
	int csl[4], msg[4];
    struct pman_msg pman_cmd;
    struct pman_res pman_res;
    int id;

    open_port(0, 0, UNRESTRICTED);

    /* Let pman know we where started */
    send_msg(PMAN_TASK, 5, &csl);
	
	while (get_msg_count(0)==0);
            
    // get command line
	get_msg(0, csl, &id);

    read_mem(csl[1], 0, mem_size(csl[1]), cmd);

    console = csl[0];

    // try to start debugging on the first console
    pman_cmd.cmd = PMAN_CMD_DEBUG;
    pman_cmd.order = 0;
    pman_cmd.param1 = 0;
    send_msg(PMAN_TASK, PMAN_CMD_PORT, &pman_cmd);

    while (get_msg_count(0)==0);

    get_msg(0, &pman_res, &id);

    if(pman_res.param1 == -1)
    {
        printf("\nCould not start trace..");
    }
    else
    {
        // wait for break
        while (get_msg_count(0)==0)
            reschedule();
        
        get_msg(0, msg, &id);

        printf("\nBreak!");

        unsigned int reg = 0xFFFFFFFF;
        // get value of eip and set it to 
        if(ttrace_reg(32,REG_EIP,&reg,0) != SUCCESS)
            printf("\ncould not get eip!");
        else
        {
            printf("\neip: %x", reg);
            reg = 5;
            if(ttrace_reg(32,REG_EIP,&reg,1) != SUCCESS)
                printf("\ncould not set eip!");
            else
                printf("\neip set to %x", reg);            
        }

        pman_cmd.cmd = PMAN_CMD_DEBUG_END;
        send_msg(PMAN_TASK, PMAN_CMD_PORT, &pman_cmd);
        while (get_msg_count(0)==0);
        get_msg(0, &pman_res, &id);

        if(pman_res.param1 == -1)
        {
            printf("\nCould not end trace..");
        }

            printf("\nAttempt to read eip.. ");
        // try to read a register
        if(ttrace_reg(32,REG_EIP,&reg,0) != SUCCESS)
            printf("\nFAILED! it works!");
        else
            printf("\nREAD (this should have failed)");
    }
	
    send_msg(PMAN_TASK, 4, csl);
	
	while(1) {reschedule();}
} 
