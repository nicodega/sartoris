/*  
 *   Sartoris microkernel interrupt handling arch-neutral functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"
#include "sartoris/error.h"
#include "sartoris/events.h"
#include "sartoris/types.h"
#include "sartoris/syscall.h"

/* sartoris events */
int evt_task = -1;
int evt_thread = -1;
int evt_interrupt = -1;
struct port *evt_port = NULL;

/*
Events:

Message event:
	Type: SARTORIS_EVT_MSG

	This event will be triggered when active on a task and sartoris will
	send a message to the task of the thread passed on evt_set_listener.
	If there are too many messages on the port, the event interrupt will
	be raised.
    
If the port is full the interrupt will be raised twice. If the port is still full after the second interrupt, the event will be ignored.
*/

int evt_set_listener(int thread, int port, int interrupt)
{
    // check the thread belongs to the current task.
    // check the int is not an IRQ and is not mapped already
    int result = FAILURE, x;
    struct thread *thr = NULL;
    struct task *tsk = NULL;

    x = mk_enter();

    if(evt_port == NULL && thread > 0 && thread < MAX_THR)
    {
        if(TST_PTR(thread,thr))
        {
            thr = GET_PTR(thread,thr);

            if(thr->task_num == curr_task)
            {
                tsk = GET_PTR(thr->task_num,tsk);

                if(tsk->open_ports[port] != NULL)
                {
                    result = create_int_handler(interrupt, thread, 1, 0);
                    if(result == SUCCESS)
                    {
                        evt_task = curr_task;
                        evt_interrupt = interrupt;
                        evt_thread = thread;
                        evt_port = tsk->open_ports[port];
                    }
                }
                else
                {
                    set_error(SERR_INVALID_PORT);
                }
            }
            else
            {
                set_error(SERR_INVALID_THR);
            }
        }
        else
        {
            set_error(SERR_INVALID_THR);
        }
    }
    else if(evt_port && thread > 0 && thread < MAX_THR)
    {
        if(thread == evt_thread)
        {
            result = destroy_int_handler(evt_interrupt, thread);
            if(result == SUCCESS)
            {
                evt_task = -1;
                evt_interrupt = -1;
                evt_thread = -1;
                evt_port = NULL;
            }
        }
        else
        {
            set_error(SERR_INVALID_THR);
        }
    }
    else
    {
        if(!(thread > 0 && thread < MAX_THR))
            set_error(SERR_INVALID_THR);
        else if(evt_port == NULL && thread == -1)
            set_error(SERR_EVT_HANDLER_NOT_SET);
        else
            set_error(SERR_EVT_HANDLER_SET);
    }

    mk_leave(x);

    return result;
}

int evt_wait(int id, int evt, int evt_param)
{
    int result = FAILURE, x, i;
    struct task *tsk = NULL;
    struct thread *thr = NULL;
    
    if(evt_port)
    {
        switch(evt)
        {
            case SARTORIS_EVT_MSG:
                x = mk_enter();
                if(id > 0 && id < MAX_TSK && TST_PTR(id,tsk))
                {
                    tsk = GET_PTR(id,tsk);
                    // if the task already has messages, return FAILURE
                    i = 0;
	                while(i < MAX_TSK_OPEN_PORTS)
	                {
		                if((evt_param & (0x1 << i)) && tsk->open_ports[i] != NULL && tsk->open_ports[i]->total != 0) 
                        {
                            break;
                        }
                        i++;
	                }
                    if(i == MAX_TSK_OPEN_PORTS)
                    {
                        tsk->evts = 1;
                        tsk->evt_ports_mask = (unsigned int)evt_param;
                        set_error(SERR_OK);
                        result = SUCCESS;
                    }
                }
                else
                {
                    set_error(SERR_INVALID_TSK);
                }
                mk_leave(x);
                break;
            case SARTORIS_EVT_INT:
                x = mk_enter();
                if(id > 0 && id < MAX_IRQ && int_handlers[id].thr_id != -1)
                {
                    set_error(SERR_OK);
                    if((int_handlers[id].int_flags & (INT_FLAG_ACTIVE | INT_FLAG_ACTIVATED)) == 0)
                    {
                        thr = GET_PTR(int_handlers[id].thr_id,thr);
                        thr->evts = 1;
                        result = SUCCESS;
                    }
                    else if(int_handlers[id].int_flags & INT_FLAG_ACTIVATED)
                    {
                        int_handlers[id].int_flags &= ~INT_FLAG_ACTIVATED;
                    }
                }
                else
                {
                    set_error(SERR_INVALID_INTERRUPT);
                }
                mk_leave(x);
                break;
            case SARTORIS_EVT_INTS:
                // validate param structure
                if (VALIDATE_PTR(id) && VALIDATE_PTR(id + sizeof(struct ints_evt_param))) 
			    {
                    struct ints_evt_param *param = (struct ints_evt_param*)MAKE_KRN_PTR(id);
                    // this could page fault
                    int base = param->base, iid;
                    unsigned int int_mask = param->mask;
                    unsigned int int_mask2 = param->mask;
                                        
                    if(base <= MAX_IRQ - 32 && int_mask > 0)
                    {
                        x = mk_enter();
                        int i = 0;

                        while(int_mask)
                        {
                            if(int_mask & 0x1)
                            {
                                iid = base + i; 
                                if(int_handlers[iid].thr_id == -1)
                                {
                                    break;
                                }

                                if((int_handlers[iid].int_flags & (INT_FLAG_ACTIVE | INT_FLAG_ACTIVATED)) != 0)
                                {
                                    if(int_handlers[iid].int_flags & INT_FLAG_ACTIVATED)
                                    {
                                        int_handlers[iid].int_flags &= ~INT_FLAG_ACTIVATED;
                                    }
                                    break;
                                }
                            }
                            i++;
                            int_mask = (int_mask >> 1);
                        }

                        if(int_mask == 0)
                        {
                            result = SUCCESS;
                            i = 0;

                            while(i < 32 && int_mask2)
                            {
                                if(int_mask2 & 0x1)
                                {
                                    iid = base + i; 
                                    thr = GET_PTR(int_handlers[iid].thr_id,thr);
                                    thr->evts = 1;
                                }
                                i++;
                                int_mask2 = (int_mask2 >> 1);
                            }
                        }
                        else
                        {
                            set_error(SERR_INVALID_INTERRUPT);
                        }

                        mk_leave(x);
                    }
                    else
                    {
                        set_error(SERR_INVALID_INTERRUPT);
                    }
                }
                else
                {
                    set_error(SERR_INVALID_PTR);
                }
                break;
        }
    }
    else
    {
        set_error(SERR_EVT_HANDLER_NOT_SET);
    }

    return result;
}

int evt_disable(int id, int evt, int evt_param)
{
    int result = FAILURE, x;
    struct task *tsk = NULL;
    struct thread *thr = NULL;

    if(evt_port)
    {
        switch(evt)
        {
            case SARTORIS_EVT_MSG:
                x = mk_enter();
                if(id > 0 && id < MAX_TSK && TST_PTR(id,tsk))
                {
                    tsk = GET_PTR(id,tsk);
                    tsk->evt_ports_mask &= ~evt_param;
                    if(!tsk->evt_ports_mask)
                        tsk->evts = 0;
                    result = SUCCESS;
                }
                else
                {
                    set_error(SERR_INVALID_TSK);
                }
                mk_leave(x);
                break;
            case SARTORIS_EVT_INT:
                x = mk_enter();
                if(id > 0 && id < MAX_IRQ && int_handlers[id].thr_id != -1)
                {                 
                    thr = GET_PTR(int_handlers[id].thr_id,thr);   
                    thr->evts = 0;
                    result = SUCCESS;
                }
                else
                {
                    set_error(SERR_INVALID_INTERRUPT);
                }
                mk_leave(x);
                break;
            case SARTORIS_EVT_INTS:
                // validate param structure
                if (VALIDATE_PTR(id) && VALIDATE_PTR(id + sizeof(struct ints_evt_param))) 
			    {
                    struct ints_evt_param *param = (struct ints_evt_param*)MAKE_KRN_PTR(id);

                    // this could page fault
                    int base = param->base;
                    unsigned int int_mask = param->mask;
                    
                    if(base <= MAX_IRQ - 32 && int_mask > 0)
                    {
                        x = mk_enter();
                        int i = 0, iid;

                        while(int_mask)
                        {
                            if(int_mask & 0x1)
                            {
                                iid = base + i; 
                                if(int_handlers[iid].thr_id != -1)
                                {
                                    thr = GET_PTR(int_handlers[iid].thr_id,thr);
                                    thr->evts = 0;
                                    bprintf("-> evt_wait: int %i thr: %i events disabled.\n", iid, int_handlers[iid].thr_id);
                                }
                            }
                            i++;
                            int_mask = (int_mask >> 1);
                        }
                        mk_leave(x);
                    }
                    else
                    {
                        set_error(SERR_INVALID_INTERRUPT);
                    }
                }
                else
                {
                    set_error(SERR_INVALID_PTR);
                }
                break;
        }
    }
    else
    {
        set_error(SERR_EVT_HANDLER_NOT_SET);
    }

    return result;
}

/* 
Internal function, assumes the object represented by id is alive 
and we are on an atomic block.
IMPORTANT: This function might brake atomicity.
*/
void evt_raise(int id, int evt, int evt_param)
{
    struct evt_msg msg;
    struct task *tsk = NULL;
    struct thread *thr = NULL;
    int res = FAILURE, x;

    if(evt_port)
    {
        msg.evt = evt;
        msg.id = id;
        msg.param = evt_param;
        msg.param2 = 0;

        switch(evt)
        {
            case SARTORIS_EVT_MSG:
            case SARTORIS_EVT_PORT_CLOSED:
                tsk = GET_PTR(id,tsk);
                tsk->evts = 0;              // disable events
                if(enqueue(-1, evt_port, (int*)&msg) == FAILURE)
                {
                    /* Raise the event int */                    
                    arch_event_raise();

                    if(evt_port && TST_PTR(id,tsk))
                        enqueue(-1, evt_port, (int*)&msg);
                }
                break;
            case SARTORIS_EVT_INT:
                thr = GET_PTR(int_handlers[evt_param].thr_id,thr);

                struct evt_msg *emsg = (struct evt_msg*)evt_port->first;
                char doenqueue = true;
                
                if(!emsg || emsg->evt == SARTORIS_EVT_MSG)
                {
                    // can't join anything
                    doenqueue = true;
                }
                else if(emsg->evt == SARTORIS_EVT_INT)
                {
                    if(emsg->param != evt_param)
                    {
                        // try to join the events to avoid too many messages on the queue
                        int base = MAX_IRQ - 32;
                        int p = evt_param;

                        while(evt_param < base){base -= 32;}

                        if(evt_param + emsg->param - (base << 1) < 32)
                        {
                            // transform the existing message on an INTS event
                            emsg->evt = SARTORIS_EVT_INTS;
                            emsg->param2 = base;
                            emsg->param = ((0x1 << evt_param) | (0x1 << emsg->param));
                            doenqueue = false;
                        }
                    }
                    else
                    {
                        // same int, ignore it
                        doenqueue = false;
                    }
                }
                else if(emsg->evt == SARTORIS_EVT_INTS)
                {
                    if(emsg->param2 < evt_param && evt_param - emsg->param2 < 32)
                        msg.param = ((0x1 << evt_param) | emsg->param);
                    else
                        doenqueue = true;
                }
                
                if(doenqueue && enqueue(-1, evt_port, (int*)&msg) == FAILURE)
                {
                    /* Raise the event int */
                    arch_event_raise();

                    if(evt_port)
                        enqueue(-1, evt_port, (int*)&msg);
                }
                thr->evts = 0;  // disable the int event
                break;
        }
    }
}
