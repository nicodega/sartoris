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

#include <lib/iolib.h>
#include <lib/printf.h>
#include <os/pman_task.h>
#include <services/pmanager/services.h>

/*******************************************************/
/****************** FILE MAPPING FUNCT *****************/
/*******************************************************/
unsigned char file_map[0x1000] __attribute__ ((aligned (0x1000)));

void test_fmap();
void test_pmap();

void main(int argc, char **argv)
{   
    open_port(1, 2, PRIV_LEVEL_ONLY);

    test_pmap();
    
    //test_fmap();
}

void test_pmap()
{
    printf("Mapping Memory..\n");

    struct pm_msg_pmap_create msg;
    struct pm_msg_response res;
    struct pm_msg_pmap_remove rem;
    int id;

    msg.pm_type = PM_PMAP_CREATE;
    msg.req_id = 0;
    msg.response_port = 1;
    msg.flags =  PM_PMAP_WRITE; // allow write
    msg.pages = 1;
    msg.start_addr = (unsigned int)file_map;
    msg.start_phy_addr = 0xFFFFFFFF; // tell pman to give us any memory place

    printf("PMAP..");

    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

    while(get_msg_count(1) == 0){ reschedule(); }

    get_msg(1, &res, &id);

    if(res.status != PM_OK)
    {
        printf("FAILED (%i)\n", res.status);
        return;
    }
    printf("OK phy: %x\n", res.new_id_aux);

    // write memory 
    file_map[3] = 0x12;

    __asm__ __volatile__ ("xchg %%bx, %%bx" : :);

    unsigned int read = file_map[3];
    
    // close the pmap
    rem.pm_type = PM_PMAP_REMOVE;
    rem.req_id = 1;
    rem.response_port = 1;
    rem.safe = 1;                // is it safe to give this physical pages to any proc? or is it stil IO mapped.
    rem.start_addr = (unsigned int)file_map;   // linear address on task

    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

    while(get_msg_count(1) == 0){ reschedule(); }

    get_msg(1, &res, &id);

    if(res.status != PM_OK)
    {
        printf("FAILED DESTROY (%i)\n", res.status);
        return;
    }
    printf("OK\n");

    printf("PMAP Test FINISHED.. read %x\n", read);
}

void test_fmap()
{
    struct pm_msg_fmap msg;
    struct pm_msg_response res;
    struct fmap_params param;
    struct pm_msg_fmap_finish end;
    int id;

    printf("Opening FILE /testtxt.. ");

    // open a file
    FILE *f = fopen("/testtxt", "r");

    if(f == NULL)
    {
        printf("FAILED\n");
        return;
    }
    else
    {
        printf("OK\n");
    }

    // map it to a memory address
    param.fs_service = f->fs_serviceid;
	param.fileid = f->file_id;
	param.length = 0x1000;
	param.perms = 0;
	param.addr_start = (unsigned int)file_map;
	param.offset = 0;

    msg.pm_type = PM_FMAP;
    msg.req_id = 0;
    msg.response_port = 1;
    msg.params_smo = share_mem(PMAN_TASK, &param, sizeof(struct fmap_params), READ_PERM);

    printf("Mapping.. ");

    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg);

    while(get_msg_count(1) == 0){ reschedule(); }

    get_msg(1, &res, &id);

    claim_mem(msg.params_smo);

    if(res.status != PM_OK)
    {
        printf("FMAP Failed %i\n", res.status);
        return;
    }
    
    // read from it
    printf("FMAP Read %x\n", file_map);

    end.pm_type = PM_FMAP_FINISH;
    end.address = file_map;
    end.response_port = 1;
    end.req_id = 2;

    while(get_msg_count(1) == 0){ reschedule(); }

    get_msg(1, &res, &id);

    if(res.status != PM_OK)
    {
        printf("FMAP FINISH Failed %i\n", res.status);
        return;
    }
    
    printf("FMAP FINISHED\n");
}
