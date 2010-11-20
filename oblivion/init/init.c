/*  
*   Oblivion init service
*   
*   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@cvtci.com.ar
*/

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>

#include <oblivion/layout.h>

#include <drivers/screen/screen.h>
#include <services/console/console.h>

#define STACK_ADDR (SRV_SLOT_SIZE - 4)
#define EFFECTIVE_SRV_SIZE (SRV_SLOT_SIZE)

#define STOP for(;;)

void create_service(int task, int thread, int slot, int invoke_level);

void init(void) 
{
    struct csl_io_msg csl_io;
    struct task serv;
    struct thread thr;

    __asm__ ("cli" : :);

    /* create p-manager service */
    create_service(PMAN_TASK, SCHED_THR, PMAN_SLOT, 3);
        
    /* create console service */  
    create_service(CONS_TASK, CONSM_THR, CONS_SLOT, 0);

    /* create DMA manager service  */  
    create_service(DMA_MAN_TASK, DMA_MAN_THR, DMA_MAN_SLOT, 0);

    /* create fdc service */
    create_service(FDC_TASK, FDCM_THR, FDC_SLOT, 0); 

    /* create ramfs service */  
    create_service(RAMFS_TASK, RAMFS_THR, RAMFS_SLOT, 0);

    __asm__ ("sti" : :);
    run_thread(SCHED_THR); 
    STOP;
}


void create_service(int task, int thread, int slot, int invoke_level) 
{
    struct task srv;
    struct thread thr;

    int i;
    unsigned int linear, physical;
    
    srv.mem_adr = (void*)MIN_TASK_OFFSET;
    srv.size = (int)EFFECTIVE_SRV_SIZE;
    srv.priv_level = 0;
    
    if (create_task(task, &srv) < 0) STOP;
    
    /* first: the page directory */
    page_in(task, 0, (void*)(SRV_PDIR_BASE + task * 0x1000), 0, 0);
        
    /* second: the page table */
    page_in(task, (void*)MIN_TASK_OFFSET, (void*)(SRV_PTAB_BASE + task * 0x1000), 1, PGATT_WRITE_ENA);	

    linear = MIN_TASK_OFFSET;
    physical = SRV_MEM_BASE + slot*SRV_SLOT_SIZE;

    for(i=0; i<(SRV_SLOT_SIZE/PAGE_SIZE); i++) 
    {
        page_in(task, (void*) linear, (void*) physical, 2, PGATT_WRITE_ENA);
        linear += PAGE_SIZE;
        physical += PAGE_SIZE;
    }
    
    /* Initialize the task */
    if (init_task(task, (void*)(INIT_CODE_SIZE + slot*SRV_DISK_SIZE), EFFECTIVE_SRV_SIZE)) STOP;
    
    thr.task_num = task;
    thr.invoke_mode = PRIV_LEVEL_ONLY;
    thr.invoke_level = invoke_level;
    thr.ep = 0;
    thr.stack = (void*)STACK_ADDR;

    if (create_thread(thread, &thr) < 0)
    {
        string_print("INIT: Could not create Thread", 0, 0x7);
        STOP;
    }
}

