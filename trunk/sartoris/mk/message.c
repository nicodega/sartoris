
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
#include "lib/bitops.h"
#include "sartoris/kernel-data.h"
#include "lib/salloc.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>

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
                    p->perms = NULL;
					p->mode = mode;
					p->priv = priv;
						    					
					task->open_ports[port] = p;

#ifdef _METRICS_
					metrics.ports++;
#endif
					result = SUCCESS;
				}
			}
	
			mk_leave(x); /* exit critical block */
		}
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
		}
      
		mk_leave(x); /* exit critical block */
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
		}
      
		mk_leave(x); /* exit critical block */
    }
    
    return result;
}

int set_port_perm(int port, struct port_perms *perms) 
{
	int x, result;
	struct task *task;
    unsigned int len;
    
	result = FAILURE;

    if(VALIDATE_PTR(perms) && VALIDATE_PTR((unsigned int)perms + 4))
    {
        // this could produce a page fault..
        len = perms->length;
    }
    else
    {
        return FAILURE;
    }

    if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{ 
        x = mk_enter(); /* enter critical block */
        
        if(len <= BITMAP_SIZE(MAX_THR) && VALIDATE_PTR((unsigned int)perms + sizeof(unsigned int) + len))
        {
            task = GET_PTR(curr_task,tsk);
    		    
		    if (task->open_ports[port] != NULL) 
		    {
			    result = SUCCESS;
			    task->open_ports[port]->perms = perms;
		    }          
        }
    
		mk_leave(x); /* exit critical block */
	}
  
	return result;
}
int msgc = 0;
int send_msg(int dest_task_id, int port, int *msg) 
{
    struct port *p;
    int x, result;
	struct task *task;
    unsigned int *perms;
    
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
            // NOTE: this could produce a page fault because it's a user space pointer
            if(p->mode == PERM_REQ && p->perms != NULL)
            {
                /*
                    p->perms is on the desination task address space. We must ask
                    the arch dependant part of the kernel to map it to our thread mapping zone!!!
                */
#ifdef PAGING
                perms = (unsigned int*)map_address(dest_task_id, (unsigned int*)p->perms + 1);
#else
                perms = ((unsigned int*)p->perms + 1);
#endif
                // we could get a page fault on getbit
                if(p->perms->length >= BITMAP_SIZE(MAX_TSK) && getbit(perms, curr_task) )
                {
                    mk_leave(x);
	                return FAILURE;
                }
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
#ifdef _METRICS_
						    if(result == SUCCESS) metrics.messages++;
#endif
					    }
				    }
			    }
		    }
        }
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
#ifdef _METRICS_
				if(result == SUCCESS) metrics.messages--;
#endif
			}
		}
    }

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
		res = FAILURE;
    else
		res = task->open_ports[port]->total;

	mk_leave(x);

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
      
    if (m != NULL) 
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
