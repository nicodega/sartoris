/*  
*   Oblivion 0.1 process manager (and shell, for now) service
*   
*   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@cvtci.com.ar
*/

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <oblivion/layout.h>
#include <services/console/console.h>
#include <services/ramfs/ramfs.h>
#include <services/filesys/filesys.h>
#include <drivers/pit/pit.h>
#include <lib/reboot.h>
#include <lib/int.h>

#include "pman_internals.h"
#include "exception.h"


#include <drivers/screen/screen.h>
#include <lib/print.h>
char txt_init[] = "\nOblivion Test Version\nMicrokernel version 2.0\n";

char *txt_csl[] = { "console 0 ready.",
"console 1 ready.",
"console 2 ready.",
"console 3 ready.",
"console 4 ready.",
"console 5 ready.",
"console 6 ready.",
"console 7 ready."};

char txt_prompt[] = "\n$";

char txt_err_smo[] = "\nprocess manager -> error while loading program.";
char txt_err_unrec[] = "\nprocess manager -> bad command or missing program file.";

char txt_server_error[] = "\n\nprocess manager -> server thread xx has died (!!)\n\n";

char txt_gpf[] = "\nprocess manager -> fatal error: program received general protection fault"; 
char txt_pgf[] = "\nprocess manager -> fatal error: program received page fault exception";
char txt_div_zero[] = "\nprocess manager -> fatal error: program received divide by zero exception";
char txt_ovflw[] =  "\nprocess manager -> warning: program received overflow exception";
char txt_bound[] =  "\nprocess manager -> fatal error: program received BOUND range exceeded exception";
char txt_inv_opcode[] = "\nprocess manager -> fatal error: program received invalid opcode exception";
char txt_dev_not_avl[] = "\nprocess manager -> fatal error: program received device not available exception";
char txt_stack_fault[] = "\nprocess manager -> fatal error: program received stack segment fault";
char txt_fp_err[] = "\nprocess manager -> fatal error: program received floating-point error exception";
char txt_alig_chk[] = "\nprocess manager -> warning: program received alignment check exception";
char txt_simd_fault[] = "\nprocess manager -> fatal error: program received SIMD extensions fault";

char txt_input[NUM_PROC][INPUT_LENGTH];
int input_smo[NUM_PROC];

int command_smo[NUM_PROC];
char restore_cmd[NUM_PROC];

char load_buffer[NUM_PROC][BLOCK_SIZE];

char serv_present[NUM_SERV_THR];

int state[NUM_PROC];

int running;

void process_manager(void) 
{
    struct csl_signal_msg csl_sgn;
    struct csl_response_msg csl_res;
    struct fs_response fs_res;

    int null_msg[4], csl[4];
    int id;
    int i, j;
    unsigned int linear, physical;

    __asm__ ("cli" : :);
/*
    if(create_int_handler(32, SCHED_THR, 0, 0) < 0)
    {
        string_print("PMAN: HANDLER CREATION FAILED", 0, 0x7); for(;;);
        STOP;
    }
*/     
    for(i=0; i<NUM_PROC; i++) 
    {
        /* first: the page directory */
        page_in(BASE_PROC_TSK + i, 0, (void*)(PRC_PDIR_BASE + i*0x1000), 0, 0);

        /* second: the page table */
        page_in(BASE_PROC_TSK + i, (void*)SARTORIS_PROC_BASE_LINEAR, (void*)(PRC_PTAB_BASE + i*0x1000), 1, PGATT_WRITE_ENA);	

        linear = SARTORIS_PROC_BASE_LINEAR;
        physical = PRC_MEM_BASE + i*PRC_SLOT_SIZE;

        for(j=0; j<(PRC_SLOT_SIZE/PAGE_SIZE); j++) 
        {
            page_in(BASE_PROC_TSK + i, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
            linear += PAGE_SIZE;
            physical += PAGE_SIZE; 
        }
    }
    
    /* get rid of the init stuff */
    destroy_thread(INIT_THREAD_NUM);
    destroy_task(INIT_TASK_NUM);
    
    spawn_handlers();

    adjust_pit(SCHED_HERTZ);

    for(i=0; i<NUM_SERV_THR; i++) 
    {
        serv_present[i]=0;
    }

    serv_present[CONSM_THR] = 1;
    serv_present[RAMFS_THR] = 1;/*
    serv_present[DMA_MAN_THR] = 1;
    serv_present[FDCM_THR] = 1;*/
    
    for (i=0; i<NUM_PROC; i++) 
    {
        input_smo[i] = share_mem(CONS_TASK, txt_input[i], INPUT_LENGTH, WRITE_PERM);
        command_smo[i] = -1;
    }

    for (i=0; i<NUM_PROC; i++) 
    {
        state[i] = UNINITIALIZED;
    }

    ack_int_master();

    /* open ports */
    open_port(CSL_SGN_PORT, 0, UNRESTRICTED);
    open_port(CSL_ACK_PORT, 0, UNRESTRICTED);
    open_port(RAMFS_INPUT_PORT, 0, UNRESTRICTED);
    open_port(TERM_REQ_PORT, 0, UNRESTRICTED);
    open_port(RUNNING_PORT, 0, UNRESTRICTED);

    int k = 7;
    for(;;) 
    {        
        string_print("PMAN ALIVE",23*160,k++);

        /* run the services */
        for (i=0; i<NUM_SERV_THR; i++) 
        {
            if (serv_present[i]) 
            {
                running = i;
                run_thread(i);
                ack_int_master();
            }
        }

        /* run the user programs */
        for (i=0; i<NUM_PROC; i++) 
        {
            if (state[i]==RUNNING) 
            {
                running = BASE_PROC_THR+i;
                run_thread(BASE_PROC_THR+i);
                ack_int_master();
            }
        }

        /* get signals from console */  
        while (get_msg_count(CSL_SGN_PORT)>0) 
        {
            get_msg(CSL_SGN_PORT, &csl_sgn, &id);
            
            if (id == CONS_TASK) 
            {
                if(csl_sgn.term == -1)
                {                    
                    for (i=0; i<NUM_PROC; i++) 
                        console_begin(i);
                }
                else if (csl_sgn.alt && csl_sgn.key == '.') 
                {
                    reboot();
                } 
                else if (state[csl_sgn.term]==RUNNING && (csl_sgn.key == 'c' || csl_sgn.key == 'C')) 
                {
                    do_unload(csl_sgn.term);
                    show_prompt(csl_sgn.term);
                }
            }          
        }

        ///* get acks from console input */
        while (get_msg_count(CSL_ACK_PORT)>0) 
        {
            get_msg(CSL_ACK_PORT, &csl_res, &id);
            if (id == CONS_TASK) 
            {
                i=csl_res.code;
                if (i>=NUM_PROC) claim_mem(csl_res.smo);
                if (0 <= i && i < NUM_PROC) 
                {
                    switch(state[i]) 
                    {
                      case PROMPT:
                          if (txt_input[i][0]) 
                          {
                              get_input(i);
                          } else {
                              show_prompt(i);
                          }
                          break;
                      case KILLED:
                          do_unload(i);
                          show_prompt(i);
                    }
                }   
            }
        }

        ///* get acks from ram-filesystem */
        while (get_msg_count(RAMFS_INPUT_PORT) > 0) 
        {
            get_msg(RAMFS_INPUT_PORT, &fs_res, &id);
            i=fs_res.id;
            if (0 <= i && i < NUM_PROC)
            {
                switch(state[i])
                {
                    case LOADING:
                        claim_mem(fs_res.smo_name);
                        claim_mem(fs_res.smo_buff);
                        if (fs_res.result<0) 
                        {
                            inform_load_error(i, FS_NO_SUCH_FILE);
                        } 
                        else 
                        {
                            do_load(i);
                        }
                        break;
                }
            }
        }

        /* get terminate requests */
        while (get_msg_count(TERM_REQ_PORT) > 0) 
        {
            get_msg(TERM_REQ_PORT, &null_msg, &i);
            if (BASE_PROC_TSK <= i && i < BASE_PROC_TSK+NUM_PROC) 
            {
                i -= BASE_PROC_TSK;
                switch(state[i]) 
                {
                    case RUNNING:
                        do_unload(i);
                        show_prompt(i);
                        break;
                }
            }
        }

        /* Get ready messages from programs (for command line) */
        while (get_msg_count(RUNNING_PORT) > 0) 
        {
            get_msg(RUNNING_PORT, &null_msg, &i);
            if (BASE_PROC_TSK <= i && i < BASE_PROC_TSK+NUM_PROC) 
            {
                i -= BASE_PROC_TSK;
                switch(state[i]) 
                {
                    case INITIALIZING:
                        
                    csl[0] = i-BASE_PROC_TSK;
                    csl[1] = command_smo[i-BASE_PROC_TSK]; 
                    send_msg(i, 0, csl);

                    state[i-BASE_PROC_TSK] = RUNNING;
                    break;
                }
            }
        }      
    }
}

int strlen(char *s)
{
    int l = 0;
    while(s[l]){l++;}
    return l;
}

void console_begin(int term) 
{
    struct csl_io_msg csl_io;

    state[term]=UNINITIALIZED;
    
    csl_io.command = CSL_RESET;
    send_msg(CONS_TASK, 1+term, &csl_io);
    
    csl_io.command = CSL_WRITE;
    csl_io.attribute = 0x7;
    csl_io.response_code = NUM_PROC+1;

    csl_io.smo = share_mem(CONS_TASK, txt_init, strlen(txt_init)+1, READ_PERM);
    send_msg(CONS_TASK, 1+term, &csl_io);
    
    csl_io.smo = share_mem(CONS_TASK, txt_csl[term], strlen(txt_csl[term])+1, READ_PERM);
    send_msg(CONS_TASK, 1+term, &csl_io);

    show_prompt(term);
}

void show_prompt(int term) 
{
    struct csl_io_msg csl_io;
    int i;
    
    csl_io.command = CSL_WRITE;
    csl_io.smo = share_mem(CONS_TASK, txt_prompt, strlen(txt_prompt)+1, READ_PERM);
    csl_io.attribute = 0x7;
    csl_io.response_code = NUM_PROC+1;
    send_msg(CONS_TASK, 1+term, &csl_io);

    for (i=0; i<INPUT_LENGTH; i++) 
    {
        txt_input[term][i] = '\0';
    }
    
    csl_io.command = CSL_READ;
    csl_io.delimiter = '\n';
    csl_io.echo = 1;
    csl_io.response_code = term;
    csl_io.smo = input_smo[term];
    send_msg(CONS_TASK, 1+term, &csl_io);

    state[term]=PROMPT;
}

void get_input(int term) 
{
    int i;

    restore_cmd[term] = 0;
    for (i=0; i<INPUT_LENGTH; i++)
    {
        if (txt_input[term][i] == ' ') 
        {
            txt_input[term][i] = '\0';
            restore_cmd[term] = 1;
            break;
        }
    }
    fetch_file(term, RAMFS_TASK, RAMFS_INPUT_PORT);
}

void fetch_file(int term, int fs_task, int ret_port) 
{    
    struct fs_command io_msg;

    state[term] = LOADING;

    io_msg.op = FS_READ;
    io_msg.smo_name = share_mem(fs_task, txt_input[term], INPUT_LENGTH, READ_PERM);
    io_msg.smo_buff = share_mem(fs_task, load_buffer[term], BLOCK_SIZE, WRITE_PERM);
    io_msg.id = term;
    io_msg.ret_port = ret_port;

    send_msg(fs_task, FS_CMD_PORT, &io_msg);
}

void inform_load_error(int term, int status) 
{    
    struct csl_io_msg csl_io;

    csl_io.command = CSL_WRITE;
    csl_io.response_code = NUM_PROC+1;
    csl_io.attribute = 0x7;
    switch (status) 
    {
      case FS_NO_SUCH_FILE:
          csl_io.smo = share_mem(CONS_TASK, txt_err_unrec, strlen(txt_err_unrec)+1, READ_PERM);
          send_msg(CONS_TASK, 1+term, &csl_io);
          break;
      case FS_FAIL:
          csl_io.smo = share_mem(CONS_TASK, txt_err_smo, strlen(txt_err_smo)+1, READ_PERM);
          send_msg(CONS_TASK, 1+term, &csl_io);
          break;
    }
}


/* FIXME (do_load): when paging is enabled, do_load should take the last two pages
out of the program segment (they are page tables) */

void do_load(int term) 
{
    struct task prc_task;
    struct thread prc_thread;
    int csl[4];
    int i;

    /* restore the first blank, that was removed
    * when the filesystem was called
    */
    if (restore_cmd[term]) 
    {
        for (i=0; i<80; i++)
        {
            if (txt_input[term][i] == '\0') 
            {
                txt_input[term][i] = ' ';
                break;
            }
        }
    }

    prc_task.mem_adr = (void*)SARTORIS_PROC_BASE_LINEAR;

    prc_task.size = PRC_SLOT_SIZE;
    prc_task.priv_level = 2;

    if (create_task(BASE_PROC_TSK+term, &prc_task)) STOP;
    if (init_task(BASE_PROC_TSK+term, (void*) load_buffer[term], BLOCK_SIZE)) STOP;

    /* pass the command line on */
    if (command_smo[term] != -1) 
    {
        claim_mem(command_smo[term]);
    }

    command_smo[term] = share_mem(BASE_PROC_TSK+term, txt_input[term], strlen(txt_input[term])+1, READ_PERM);

    prc_thread.task_num = BASE_PROC_TSK+term;
    prc_thread.invoke_mode = PRIV_LEVEL_ONLY;
    prc_thread.invoke_level = 0;
    prc_thread.ep = 0;
    prc_thread.stack = (void*)(0x10000 - 0x4);

    create_thread(BASE_PROC_THR+term, &prc_thread);

    state[term] = INITIALIZING;    
}

void do_unload(int term) {

    state[term] = PROMPT;

    destroy_thread(BASE_PROC_THR+term);
    destroy_task(BASE_PROC_TSK+term);
}

int prefix(char *p, char *s) 
{
    int i=0;
    while (p[i]) 
    {
        if (!s[i] || p[i]!=s[i]) return 0;
        i++;
    }

    return 1;
}

int streq(char *s, char *t) 
{
    int i=0;
    char c;

    while((c=s[i]) == t[i]) 
    {
        if (!c) return 1;
        i++;
    }

    return 0;
}

int csl_print(int csl, char *str, int att, int len, int res_code) 
{
    struct csl_io_msg csl_io;

    csl_io.command = CSL_WRITE;
    csl_io.smo = share_mem(CONS_TASK, str, len+1, READ_PERM);
    csl_io.attribute = att;
    csl_io.response_code = res_code;
    return send_msg(CONS_TASK, 8+csl, &csl_io);
}

void spawn_handlers(void) 
{
    struct thread hdl;

    hdl.task_num = PMAN_TASK;
    hdl.invoke_mode = PRIV_LEVEL_ONLY;
    hdl.invoke_level = 0;
    hdl.ep = (void*)&handler;
    hdl.stack = (void*)(0x09c00 - 0x4);

    create_thread(EXC_HANDLER_THR, &hdl);

    create_int_handler(DIV_ZERO, EXC_HANDLER_THR, false, 0);
    create_int_handler(OVERFLOW, EXC_HANDLER_THR, false, 0);
    create_int_handler(BOUND, EXC_HANDLER_THR, false, 0);
    create_int_handler(INV_OPC, EXC_HANDLER_THR, false, 0);
    create_int_handler(DEV_NOT_AV, EXC_HANDLER_THR, false, 0);
    create_int_handler(STACK_FAULT, EXC_HANDLER_THR, false, 0);
    create_int_handler(GEN_PROT, EXC_HANDLER_THR, false, 0);
    create_int_handler(PAGE_FAULT, EXC_HANDLER_THR, false, 0);
    create_int_handler(FP_ERROR, EXC_HANDLER_THR, false, 0);
    create_int_handler(ALIG_CHK, EXC_HANDLER_THR, false, 0);
    create_int_handler(SIMD_FAULT, EXC_HANDLER_THR, false, 0);
}


void handler(void) 
{
    int prog_id, j;
    int exception;
    char done;
    char *msg;

    for(;;) {

        exception = get_last_int();

        if (running >= BASE_PROC_THR) {
            prog_id = running - BASE_PROC_THR;
            state[prog_id] = KILLED;
            switch(exception) 
            {
              case DIV_ZERO:
                  msg = txt_div_zero;
                  break;
              case OVERFLOW:
                  msg = txt_ovflw;
                  break;
              case BOUND:
                  msg = txt_bound;
                  break;
              case INV_OPC:
                  msg = txt_inv_opcode;
                  break;
              case DEV_NOT_AV:
                  msg = txt_dev_not_avl;
                  break;
              case STACK_FAULT:
                  msg = txt_stack_fault;
                  break;
              case GEN_PROT:
                  msg = txt_gpf;
                  break;
              case PAGE_FAULT:
                  msg = txt_pgf;
                  break;
              case FP_ERROR:
                  msg = txt_fp_err;
                  break;
              case ALIG_CHK:
                  msg = txt_alig_chk;
                  break;
              case SIMD_FAULT:
                  msg = txt_simd_fault;
                  break;
            }
            csl_print(prog_id, msg, 0x7, 80, prog_id);

        } 
        else 
        { /* a service thread crashed! */

            txt_server_error[35] = ((running / 10) % 10) + '0';
            txt_server_error[36] = (running % 10) + '0';       

            done = 0;
            for (j=0; j<NUM_PROC; j++) 
            {
                csl_print(j, txt_server_error, 0x7, 80, running);
                done = 1;
            }

            /* if nobody is logged in, assume it's an startup error */
            /* and inform to the first console                      */
            if (!done) 
            {
                csl_print(0, txt_server_error, 0x7, 80, running);
            }

            serv_present[running] = 0;
        }
        run_thread(SCHED_THR);
    }

}
