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


#include "types.h"
#include "pman_print.h"
#include "interrupts.h"
#include "exception.h"
#include "scheduler.h"
#include "vm.h"
#include "ports.h"
#include "kmalloc.h"
#include <services/pmanager/signals.h>
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "signals.h"
#include "layout.h"
#include <services/pmanager/debug.h>

/* Container for signals waiting for a given interrupt. */
struct interrupt_signals_container interrupt_signals[MAX_INTERRUPT];
int blocked_threads[MAX_INTERRUPT];
struct pm_thread *hardint_thr_handlers[32];

void int_common_handler();
void sartoris_evt_handler();
INT32 apic = 0;
extern unsigned int *APICAddr;

void int_init()
{
	struct thread hdl;
	struct pm_thread *pmthr;
	UINT32 i;

    // check if an apic is present and setup
    __asm__ __volatile__ ("movl 0xF0(%%edx), %%eax;"
                          "andl $0x100, %%eax;"
                          "movl %%eax, %[apic]" : [apic] "=m" (apic) : "d" (APICAddr) : "eax");
        
    if(apic)
        pman_print_dbg("APIC detected\n");
    
    for(i = 0; i < MAX_INTERRUPT; i++)
		blocked_threads[i] = 0;
	
    for(i = 0; i < 32; i++)
		hardint_thr_handlers[i] = NULL;
	
	/* Create generic exceptions handler. */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &gen_ex_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_EXSTACK_ADDR);

	if(create_thread(EXC_HANDLER_THR, &hdl))
		pman_print_dbg("INT: FAILED To create Exception handler Thread");
    pman_print_dbg("OK1\n");
	create_int_handler(DIV_ZERO, EXC_HANDLER_THR, false, 0);
    pman_print_dbg("OK2\n");
	create_int_handler(OVERFLOW, EXC_HANDLER_THR, false, 0);
	create_int_handler(BOUND, EXC_HANDLER_THR, false, 0);
	create_int_handler(INV_OPC, EXC_HANDLER_THR, false, 0);
	create_int_handler(DEV_NOT_AV, EXC_HANDLER_THR, false, 0);
	create_int_handler(STACK_FAULT, EXC_HANDLER_THR, false, 0);
	create_int_handler(GEN_PROT, EXC_HANDLER_THR, false, 0);
	create_int_handler(FP_ERROR, EXC_HANDLER_THR, false, 0);
	create_int_handler(ALIG_CHK, EXC_HANDLER_THR, false, 0);
	create_int_handler(SIMD_FAULT, EXC_HANDLER_THR, false, 0);

    pmthr = thr_create(EXC_HANDLER_THR, NULL);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	// ehm... well... it IS an interrupt handler :D

	/* Create Page Fault Handler */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &vmm_paging_interrupt_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_PGSTACK_ADDR);

	if(create_thread(PGF_HANDLER_THR, &hdl))
		pman_print_dbg("INT: FAILED To create PF handler Thread");

	create_int_handler(PAGE_FAULT, PGF_HANDLER_THR, FALSE, 0);

    pmthr = thr_create(PGF_HANDLER_THR, NULL);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	

	/* Create generic interrupt stub (for threads wanting to handle the interrupt using signals) */
	hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &int_common_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_INTCOMMON_STACK_ADDR);

	if(create_thread(INT_HANDLER_THR, &hdl))
		pman_print_dbg("INT: FAILED To create Interrupt handler Thread");

	for(i = IA32FIRST_INT; i < MAX_INTERRUPT; i++)
	{
		interrupt_signals[i].first = NULL;
		interrupt_signals[i].total = 0;
	}

    pmthr = thr_create(INT_HANDLER_THR, NULL);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	

    /* Create sartoris event handler thread */
    hdl.task_num = PMAN_TASK;
	hdl.invoke_mode = PRIV_LEVEL_ONLY;
	hdl.invoke_level = 0;
	hdl.ep = &sartoris_evt_handler;
	hdl.stack = (ADDR)STACK_ADDR(PMAN_EVTHNDL_STACK_ADDR);

	if(create_thread(SART_EVT_THR, &hdl))
		pman_print_dbg("INT: FAILED To create Interrupt handler Thread.\n");

    /* Open the port for events */
    if(open_port(SARTORIS_EVENTS_PORT, 0, PRIV_LEVEL_ONLY) == FAILURE)
		pman_print_dbg("Could not open port for event listener.\n");

    /* Tell sartoris to generate events */
    if(evt_set_listener(SART_EVT_THR, SARTORIS_EVENTS_PORT, SART_EVENTS_INT) == FAILURE)
        pman_print_dbg("Could not initialize sartoris event listener.\n");

    pmthr = thr_create(SART_EVT_THR, NULL);
	pmthr->task_id = PMAN_TASK;
	pmthr->state = THR_INTHNDL;	
}

/* Generic Exceptions Handler */
void gen_ex_handler()
{
	int running_thr, exception, rval = 0;
	struct pm_thread *thr;
	struct pm_task *tsk;
    int error_code;
    void *eaddr;

	for(;;) 
	{
        error_code = 0;
		/* Get last exception raised */
        exception = get_last_int(&error_code);
        eaddr = get_last_int_addr();

		/* Get running thread */
		running_thr = sch_running();
        
		switch(exception) 
		{
            case DEBUG:
                rval = DEBUG_RVAL;
                break;
			case DIV_ZERO:
				rval = DIV_ZERO_RVAL;			
				break;
			case OVERFLOW:
				rval = OVERFLOW_RVAL;
				break;
			case BOUND:
				rval = BOUND_RVAL;
				break;
			case INV_OPC:
				rval = INV_OPC_RVAL;
				break;
			case DEV_NOT_AV:
				rval = DEV_NOT_AV_RVAL;
				break;
			case STACK_FAULT:
				rval = STACK_FAULT_RVAL;
				break;
			case GEN_PROT:
				rval = GEN_PROT_RVAL;
				break;
			case FP_ERROR:
				rval = FP_ERROR_RVAL;
				break;
			case ALIG_CHK:
				rval = ALIG_CHK_RVAL;
				break;
			case SIMD_FAULT:
				rval = SIMD_FAULT_RVAL;
				break;
		}

		if(running_thr > MAX_TSK || sch_running() == 0xFFFF)
			pman_print_and_stop("Exception on process manager, invalid running id ex=%i, id=%i", exception, running_thr);
		
		thr = thr_get(running_thr);
		tsk = tsk_get(thr->task_id);

		if(tsk == NULL)
		{
			pman_print_and_stop("EXCEPTION: NULL TASK");
			run_thread(SCHED_THR);
		}
		
        if(tsk->flags & TSK_DEBUG)
        {
            // deactivate the thread
            thr->state = THR_DBG;
            sch_deactivate(thr);

            // send a message to the debugger!
            struct dbg_exception_message dbg_msg;

            dbg_msg.command = DEBUG_EXCEPTION;
            dbg_msg.task = thr->task_id;
            dbg_msg.thread = running_thr;
            dbg_msg.exception = DEBUG_MAP_EXCEPTION(exception);
            dbg_msg.error_code = error_code;
            send_msg(tsk->dbg_task, tsk->dbg_port, &dbg_msg);
        }
        else if(!(tsk->flags & TSK_FLAG_SYS_SERVICE))
		{
            if(tsk->signals.handler_ep != NULL)
            {
                exception_signal(tsk->id, running_thr, rval, eaddr);
            }
            else
            {
			    pman_print_dbg("PROCESS EXCEPTION task %i, thread %i, exception %i at: %x \n", tsk->id, thr->id, exception, eaddr);
			
			    fatal_exception( tsk->id, rval );
            }
		}
		else
		{
            if(thr == NULL)
		    {
			    pman_print_and_stop("EXCEPTION: NULL TASK");
			    run_thread(SCHED_THR);
		    }

			pman_print_and_stop("System Service exception tsk=%i, thr=%i, ex=%i ", tsk->id, thr->id, exception);
			
            thr->state = THR_EXCEPTION;
            sch_deactivate(thr);
		}
        run_thread(SCHED_THR);
	}
}
  
BOOL is_int0_active()
{
	int res = 0;
    // if we have an apic check APIC ISR
    if(apic)
    {
        // check vector 32 ISR
        __asm__ __volatile__ ("movl 0x110(%%edx), %%eax;"
                        "andl $0x1,%%eax;"
                        "movl %%eax, %0" : "=r" (res) : "d" (APICAddr) : "eax");    
    }
    else
    {
	    // 0xB = 1011 meaning we will read the ISR (In Service Reg) from the first PIC
	    __asm__ __volatile__ ("xor %%eax,%%eax;"
                              "movb $0x0B, %%al;"
                              "outb %%al, $0x20;"
                              "inb $0x20, %%al;"
                              "andl $0x1, %%eax;"
                              "movl %%eax, %0" : "=r" (res) :: "eax");
    }
	return res;
}

/* Check if this thread can attach to this interrupt */
BOOL int_can_attach(struct pm_thread *thr, UINT32 interrupt)
{
	/* Check interrupt is not already being handled directly. */
	struct pm_thread *thread;
	UINT16 i = 0;

	/* Check interrupt is not being signaled. */
	if(interrupt_signals[interrupt].total != 0 || interrupt == 32) return FALSE; // 32 is the scheduler timer interrupt

	while(i < MAX_THR)
	{
		thread = thr_get(i);
		if(thread != NULL && thread->state == THR_INTHNDL && thread->interrupt == interrupt)
			return FALSE;
		i++;
	}

	return TRUE;
}

/* create the interrupt handler */
BOOL int_attach(struct pm_thread *thr, UINT32 interrupt, int priority)
{
	if(interrupt > 32 && interrupt < 64 && hardint_thr_handlers[interrupt-32])
		destroy_int_handler(interrupt, thr->id);

	if(create_int_handler(interrupt, thr->id, true, priority) != SUCCESS)
    {
        hardint_thr_handlers[interrupt-32] = NULL;
		return FALSE;
    }
    blocked_threads[interrupt] = 0;
    hardint_thr_handlers[interrupt-32] = thr;
	return TRUE;
}

BOOL int_dettach(struct pm_thread *thr)
{
    if(thr->interrupt > 32 && thr->interrupt < 64)
    {
        if(hardint_thr_handlers[thr->interrupt-32] == thr)
        {
            hardint_thr_handlers[thr->interrupt-32] = NULL;
            destroy_int_handler(thr->interrupt, thr->id);
        }
    }
	if(create_int_handler(thr->interrupt, INT_HANDLER_THR, true, 10) != SUCCESS)
		return FALSE;
	return TRUE;
}

UINT32 int_clear()
{
	__asm__("cli" ::);
	return 0;
}

void int_set(UINT32 x)
{
	if(x != 1) __asm__("sti" ::);
}

extern int debug_cont;
/*
Generic Interrupt handler.
This handler will send a signal to each process waiting for the interrupt.
*/
void int_common_handler()
{
	pman_print_dbg("INT COMMON HANDLER!\n");
    for(;;);
	
	INT32 interrupt, i = 0;
	struct event_cmd evt;
	struct thr_signal *isignal = NULL, *last_isignal = NULL, *next_signal = NULL;
    int last_error;
	int_clear();
	
	evt.command = EVENT;
	evt.event_type = PMAN_INTR;
	evt.task = PMAN_GLOBAL_EVENT;
	evt.event_res = 0;

	for(;;)
	{
		interrupt = get_last_int(&last_error);

		/* Find tasks waiting for an interrupt signal, and send it. */
		if(interrupt_signals[interrupt].total != 0)
		{
			evt.param = interrupt;

			isignal = interrupt_signals[interrupt].first;

			while(isignal != NULL)
			{
				/* Send the signal */
				send_signal(isignal, &evt, SIGNAL_OK);

                /* Update thread status */
	            if(isignal->thread->signals.blocking_signal == isignal)
	            {
                    if(isignal->rec_type != SIGNAL_REC_TYPE_REPEATING)
                        isignal->thread->signals.blocking_signal = NULL;

		            /* Reactivate thread */
		            sch_activate(isignal->thread);
	            }

				/* Reschedule thread so it executes before other threads. */
				sch_reschedule(isignal->thread, i);
				
				next_signal = isignal->inext;

				/* Discard signal if not repeating */
				if(isignal->rec_type != SIGNAL_REC_TYPE_REPEATING)
				{
					remove_signal(isignal, isignal->thread);

					/* Remove from interrupt signals */
					if(last_isignal == NULL)
						interrupt_signals[interrupt].first = isignal->inext; 	/* Signal is first on the list */					
					else
						last_isignal->inext = isignal->inext;
					
					interrupt_signals[interrupt].total--;

					/* Free signals */
					kfree(isignal);					
				}
				else
				{
					last_isignal = isignal;
				}

				isignal = next_signal;

				i++;
			}
		}

		/* FIX: Ack PICs if int is from hardware */

		/* Tell Scheduler to move our current executing thread */
		sch_force_complete();

		ret_from_int(0);
	}
}

BOOL int_signal(struct pm_thread *thread, struct thr_signal *signal)
{
    int interrupt = signal->signal_param;

	if(int_can_attach(thread, interrupt))
	{
		/* Add signal to the list */
		if(interrupt_signals[interrupt].first == NULL)
		{
			interrupt_signals[interrupt].first = signal;
			signal->inext = NULL;
		}
		else
		{
			signal->inext = interrupt_signals[interrupt].first;
			interrupt_signals[interrupt].first = signal;
		}

		interrupt_signals[interrupt].total++;

		return TRUE;
	}
	return FALSE;
}

void int_signal_remove(struct thr_signal *signal)
{
    struct thr_signal *is = interrupt_signals[signal->signal_param].first,
            *ois = NULL;

    while(is != signal)
    {            
        ois = is;
        is = is->inext;
    }

    if(is == signal)
    {
        if(ois) 
            ois->inext = is->inext;
        else
            interrupt_signals[signal->signal_param].first = is->inext;
    }
}

void sartoris_evt_handler()
{
    for(;;)
    {
        sch_process_portblocks();

        ret_from_int(0);
    }
}
