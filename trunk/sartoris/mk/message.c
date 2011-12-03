
/*  
 *   Sartoris messaging subsystem implementation
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "sartoris/metrics.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "sartoris/kernel-data.h"
#include "lib/salloc.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/error.h"
#include "sartoris/permissions.h"

/* message subsystem implementation */

/* these are the proper system calls: */
int open_port(int port, int priv, enum usage_mode mode) 
{
    int i;
    int x, result;
	struct task *task;
	struct port *p;
	
    result = FAILURE;
	
    if (0 <= mode && mode <= MAX_USAGE_MODE) 
	{
		if (0 <= port && port <= MAX_TSK_OPEN_PORTS) 
		{	
			x = mk_enter(); /* enter critical block */
	
			task = GET_PTR(curr_task,tsk);

			if (task->open_ports[port] == NULL) 
			{
	   			p = create_port(task); /* look up a free port */
	  
				if (p != NULL) 
				{   
                    p->mode = mode;
					p->priv = priv;
				
                    init_perms(&p->perms);

					task->open_ports[port] = p;

#ifdef _METRICS_
					metrics.ports++;
#endif
					result = SUCCESS;
                    set_error(SERR_OK);
				}
			}
            else
            {
                set_error(SERR_INVALID_PORT);
            }
	
			mk_leave(x); /* exit critical block */
		}
        else
        {
            set_error(SERR_INVALID_PORT);
        }
    }
    else
    {
        set_error(SERR_INVALID_MODE);
    }
      
    return result;
}

int close_port(int port) 
{
    int p, result, x;
	struct task *task;
	    
    result = FAILURE;
    
    if (0 <= port && port <= MAX_TSK_OPEN_PORTS) 
	{
		x = mk_enter(); /* enter critical block */
    
		task = GET_PTR(curr_task,tsk);
		
		if(task->open_ports[port] != NULL) 
		{
			result = SUCCESS;
	
			delete_port(task, task->open_ports[port]);
			task->open_ports[port] = NULL;

#ifdef _METRICS_
			metrics.ports--;
#endif
            set_error(SERR_OK);
		}
        else
        {
            set_error(SERR_INVALID_PORT);
        }
      
		mk_leave(x); /* exit critical block */
    }
    else
    {
        set_error(SERR_INVALID_PORT);
    }

    return result;
}

int set_port_mode(int port, int priv, enum usage_mode mode) 
{
    int p, x, result;
	struct task *task;
    
    result = FAILURE;
  
    if (0 <= mode && mode <= MAX_USAGE_MODE) 
	{
		x = mk_enter(); /* enter critical block */
		
		task = GET_PTR(curr_task,tsk);
		
		if (task->open_ports[port] != NULL) 
		{
			result = SUCCESS;
			task->open_ports[port]->priv = priv;
			task->open_ports[port]->mode = mode;
            set_error(SERR_OK);
		}
        else
        {
            set_error(SERR_INVALID_PORT);
        }
      
		mk_leave(x); /* exit critical block */
    }
    else
    {
        set_error(SERR_INVALID_MODE);
    }
    
    return result;
}

int set_port_perm(int port, struct permissions *perms) 
{
	int x, result;
	struct task *task;
    
    result = FAILURE;

    x = mk_enter(); /* enter critical block */
    
    if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
    {
        task = GET_PTR(curr_task,tsk);
    		    
		if (task->open_ports[port] != NULL) 
		{
            if(validate_perms_ptr(perms, &task->open_ports[port]->perms, MAX_TSK, -1))
	        {
                result = SUCCESS;
            }
            else
            {
                init_perms(&task->open_ports[port]->perms);
            }
		}
        else
        {
            set_error(SERR_INVALID_PORT);
        }
    }
    else
    {
        set_error(SERR_INVALID_PORT);
    }
	
    mk_leave(x); /* exit critical block */
  
	return result;
}

int send_msg(int dest_task_id, int port, int *msg) 
{
    struct port *p = NULL;
    int x, result;
	struct task *task = NULL;
    
    result = FAILURE;
    
	x = mk_enter(); /* enter critical block */
    
    if (0 <= dest_task_id && dest_task_id < MAX_TSK && 
        TST_PTR(dest_task_id,tsk) && 0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{
		task = GET_PTR(dest_task_id,tsk);
        
        p = task->open_ports[port];

        if(p != NULL)
        {
            // if invoke move is PERM_REQ, and the user loaded a bitmap for permissions
            // check for permissions.
            // NOTE: this could produce a page fault because it's a user space pointer, but 
            // only on threads with privilege level greater than 0
            if(curr_priv != 0 
                && p->mode == PERM_REQ 
                && p->perms.bitmap != NULL
                && !test_permission(dest_task_id, &p->perms, port))
            {
                mk_leave(x);
	            return FAILURE;
            }

            // check destination port is still open, and task is alive, because the page fault occurred.
		    if (TST_PTR(dest_task_id,tsk) && task->state == ALIVE && task->open_ports[port] != NULL) 
		    {
                p = task->open_ports[port];

			    if (VALIDATE_PTR(msg)) 
			    {
				    p = task->open_ports[port];

				    if (p->mode != DISABLED && p->total < MAX_MSG_ON_PORT) 
				    {					
				        if (p->mode == UNRESTRICTED || curr_priv <= p->priv) 
					    {
						    result = enqueue(curr_task, p, (int *) MAKE_KRN_PTR(msg));

						    if(result == SUCCESS)
                            {
                                set_error(SERR_OK);
#ifdef _METRICS_
                                metrics.messages++;
#endif
                            }
					    }
                        else
                        {
                            set_error(SERR_NO_PERMISSION);
                        }
				    }
                    else
                    {
                        if(p->mode != DISABLED)
                            set_error(SERR_PORT_DISABLED);
                        else
                            set_error(SERR_PORT_FULL);
                    }
			    }
                else
                {
                    set_error(SERR_INVALID_PTR);
                }
		    }
            else
            {
                if(!TST_PTR(dest_task_id,tsk) || task->state != ALIVE)
                    set_error(SERR_INVALID_TSK);
                else
                    set_error(SERR_INVALID_PORT);
            }
        }
        else
        {
            set_error(SERR_INVALID_PORT);
        }
    }
    else
    {
        if(0 > dest_task_id || dest_task_id >= MAX_TSK)
            set_error(SERR_INVALID_ID);
        else if(TST_PTR(dest_task_id,tsk))
            set_error(SERR_INVALID_TSK);
        else if(0 <= port && port < MAX_TSK_OPEN_PORTS)
            set_error(SERR_INVALID_PORT);
    }
    mk_leave(x); /* exit critical block */
    
    return result;
}

int get_msg(int port, int *msg, int *id) 
{
    struct port *p;
	struct task *task;
    int x, result;
	
    result = FAILURE;

    x = mk_enter(); /* enter critical block */
      
    if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{
		task = GET_PTR(curr_task,tsk);
		p = task->open_ports[port];
      
		if (p != NULL) 
		{
			if (VALIDATE_PTR(msg) && VALIDATE_PTR(id)) 
			{
				result = dequeue((int *)MAKE_KRN_PTR(id), p, (int *)MAKE_KRN_PTR(msg));

				if(result == SUCCESS) 
                {
#ifdef _METRICS_
                    metrics.messages--;
#endif
                    set_error(SERR_OK);
                }
			}
            else
            {
                set_error(SERR_INVALID_PTR);
            }
		}
        else
        {
            set_error(SERR_INVALID_PORT);
        }
    }
    else
    {
        set_error(SERR_INVALID_PORT);
    }

    //if(curr_task == 6) kprintf(12, "                           GMSG task %i From %i, port %i\n", curr_task, *((int*)MAKE_KRN_PTR(id)), port);
    mk_leave(x); /* exit critical block */
    
    return result;
}

int get_msg_count(int port) 
{
	int x, res;
	struct task *task;

	x = mk_enter();

	task = GET_PTR(curr_task,tsk);
    if (0 > port || port >= MAX_TSK_OPEN_PORTS || task->open_ports[port] == NULL)
    {
        set_error(SERR_INVALID_PORT);
		res = FAILURE;
    }
    else
    {
        set_error(SERR_OK);
		res = task->open_ports[port]->total;
    }

	mk_leave(x);

	return res;
}

/*
This function will place on msgs as many as maxlen messages, if there are any on the specificed port.
It will return how many messages where read onto msgs.
*/
int get_msgs(int port, int *msgs, int *ids, int maxlen) 
{
    struct port *p;
	struct task *task = NULL;
    int x, result, res, i;
	
    result = 0;

    x = mk_enter(); 
    
    if(VALIDATE_PTR(msgs) && VALIDATE_PTR(((unsigned int)msgs)+(maxlen*MSG_LEN)) && VALIDATE_PTR(((unsigned int)ids)+maxlen) && VALIDATE_PTR(ids))
    {        
        msgs = MAKE_KRN_PTR(msgs);
        ids = MAKE_KRN_PTR(ids);
        task = GET_PTR(curr_task,tsk);
		        
        set_error(SERR_INVALID_PORT);

        if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
	    {
		    p = task->open_ports[port];
      
		    if (p != NULL) 
		    {
                set_error(SERR_OK);
                
                if(p->total < maxlen)
                    maxlen = p->total;

                for(i = 0; i < maxlen; i++)
                {                    
			        res = dequeue(&ids[i], p, &msgs[i+MSG_LEN]);

#ifdef _METRICS_
                    if(res == SUCCESS) 
                        metrics.messages--;
#endif              
                }

                result = i;
		    }
        }
    }
    else
    {       
        set_error(SERR_INVALID_PTR);
    }
	
    mk_leave(x); /* exit critical block */
    
    return result;
}

/*
This function will return a value >0 if there are messages on any of the ports specified on the ports array, and will
set the counts on the specified array.
It will return a negative number if an error occured.
*/
int get_msg_counts(int *ports, int *counts, int len) 
{
	int x, res = -1, i;
	struct task *task;

    if(VALIDATE_PTR(ports) && VALIDATE_PTR(((unsigned int)ports)+len) && VALIDATE_PTR(((unsigned int)counts)+len) && VALIDATE_PTR(counts))
    {        
        ports = MAKE_KRN_PTR(ports);
        counts = MAKE_KRN_PTR(counts);

	    task = GET_PTR(curr_task,tsk);

        set_error(SERR_OK);
        
        for(i = 0; i < len; i++)
        {
            x = mk_enter();
            
            if (0 > ports[i] || ports[i] >= MAX_TSK_OPEN_PORTS || task->open_ports[ports[i]] == NULL)
            {
                set_error(SERR_INVALID_PORT);
		        res = -1;
                break;
            }
            else
            {
                counts[i] = task->open_ports[ports[i]]->total;
                if(!res && counts[i])
                    res = 1;
            }
	    
            mk_leave(x);
        }
    }
    else
    {
        x = mk_enter();        
        set_error(SERR_INVALID_PTR);
	    mk_leave(x);
    }
	
	return res;
}

/* the following functions implement the data structures used above: */

/* this function is called within a critical block */
/* task is assumed to be in range */
struct port *create_port(struct task *task) 
{
    int new, old_first;
    int i;
	struct port *p;

    p = (struct port*)salloc(0, SALLOC_PRT);

    if (p != NULL) 
	{
		p->total = 0;
		p->last = p->first = NULL;		
    }
    else
    {
        set_error(SERR_NO_MEM);
    }
    
    return p;
}

/* 1. this function is called within a critical block */
/* 2. task is assumed to be in range                  */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open (i.e. not in the free 
      ports list) */
void delete_port(struct task *task, struct port *port) 
{
	int prev, next;

    empty(port);		/* first empty the port, move all   */
                        /* its queued messages to free list */
    
    sfree(port, 0, SALLOC_PRT);
    
    return;
}

/* 1. this function is called within a critical block */
/* 2. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 3. port should be open & good (i.e. its queued 
      messages should NOT be in the free list)        */
void empty(struct port *p) 
{
    struct message *m, *next;
      
    m = p->first;
    p->last = p->first = NULL;
    p->total = 0;
      
    while (m != NULL) 
	{
		next = m->next;
		sfree(m, 0, SALLOC_MSG);
		m = next;
    }
    
    return;
}

/* 1. this function is called within a critical block */
/* 2. from_task_id is assumed to be within range      */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open & good                      */
int enqueue(int from_task_id, struct port *p, int *msg) 
{
	struct message *m;
    int *dst, result, i;
    
    result = FAILURE;
    
    m = (struct message *)salloc(0, SALLOC_MSG);

    if (m != NULL) 
	{
		result = SUCCESS;
      
		dst = (int*)m->data;
		for (i = 0; i < MSG_LEN; i++) 
			dst[i] = msg[i];
		
		m->next = NULL;
		m->sender_id = from_task_id;
      
		/* Insert message at the end of the port */
      	if(p->first == NULL)
		{
			p->first = m;
            p->last = m;
		}
		else
		{
			p->last->next = m;
			p->last = m;
		}
		p->total++;
    }
    else
    {
        set_error(SERR_NO_MEM);
    }
    
    return result;
}

/* 1. this function is called within a critical block */
/* 2. pointers are assumed to be good (id and msg)    */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open & good                      */
int dequeue(int *id, struct port *p, int *msg) 
{
    int i, result;
    struct message *first;
    int *src;
    
    result = FAILURE;

    first = p->first;

    if (first != NULL) 
	{
		result = SUCCESS;
      
		p->first = first->next;
		
		*id = first->sender_id;

		src = (int*)first->data;
		for (i = 0; i < MSG_LEN; i++) 
		{
			msg[i] = src[i];
		}
      
		p->total--;
		if (p->total == 0) 
		{
			p->last = NULL;
		}

        sfree(first, 0, SALLOC_MSG);
    }
    else
    {
        set_error(SERR_NO_MSG);
    }

    return result;
}

void delete_task_ports(struct task *task) 
{
    int i, j, x;
    int old_free_first;

    x = mk_enter(); /* enter critical block */

    i = 0;

	while(i < MAX_TSK_OPEN_PORTS)
	{
		if(task->open_ports[i] != NULL) 
            delete_port(task, task->open_ports[i]);
        i++;
	}
        
    mk_leave(x); /* exit critical block */
}
