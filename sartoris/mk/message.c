
/*  
 *   Sartoris messaging subsystem implementation
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@cvtci.com.ar      
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

/* message subsystem implementation */

/* these are the proper system calls: */
int open_port(int port, int priv, enum usage_mode mode) 
{
    int i;
    int x, result;
	struct task *task;
	struct port *p;
	
    result = FAILURE;

    if (0 <= mode && mode < MAX_USAGE_MODE) 
	{
		if (0 <= port && port <= MAX_TSK_OPEN_PORTS) 
		{	
			x = arch_cli(); /* enter critical block */
	
			task = (struct task*)GET_PTR(curr_task,tsk);

			if (task->open_ports[port] == NULL) 
			{				
	   			p = create_port(task); /* look up a free port */
	  
				if (p != NULL) 
				{
					/* initialize port permissions */
					for (i = 0; i < (BITMAP_SIZE(MAX_TSK)); i++) 
					{
						p->perms[i] = 0;
					}
	    
					p->mode = mode;
					p->priv = priv;
	    					
					task->open_ports[port] = p;

#ifdef _METRICS_
					metrics.ports++;
#endif
					result = SUCCESS;
				}
			}
	
			arch_sti(x); /* exit critical block */
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
		x = arch_cli(); /* enter critical block */
    
		task = (struct task*)GET_PTR(curr_task,tsk);

		if(task->open_ports[port] != NULL) 
		{
			result = SUCCESS;
	
			delete_port(task, task->open_ports[port]);
			task->open_ports[port] = NULL;

#ifdef _METRICS_
			metrics.ports--;
#endif
		}
      
		arch_sti(x); /* exit critical block */
    }

    return result;
}

int set_port_mode(int port, int priv, enum usage_mode mode) 
{
    int p, x, result;
	struct task *task;
    
    result = FAILURE;
  
    if (0 <= mode && mode < MAX_USAGE_MODE) 
	{
		x = arch_cli(); /* enter critical block */
		
		task = (struct task*)GET_PTR(curr_task,tsk);
		
		if (task->open_ports[port] != NULL) 
		{
			result = SUCCESS;
			task->open_ports[port]->priv = priv;
			task->open_ports[port]->mode = mode;
		}
      
		arch_sti(x); /* exit critical block */
    }
    
    return result;
}

int set_port_perm(int port, int task_id, int perm) 
{
	int x, result;
	struct task *task;
    
	result = FAILURE;
  
	if (0 <= task_id && task_id < MAX_TSK && 0 <= port && port < MAX_TSK_OPEN_PORTS && !(perm & ~(1))) 
	{
	    x = arch_cli(); /* enter critical block */
    
		task = (struct task*)GET_PTR(curr_task,tsk);
		    
		if (task->open_ports[port] != NULL) 
		{
			result = SUCCESS;
			setbit(task->open_ports[port]->perms, task_id, perm);
		}
    
		arch_sti(x); /* exit critical block */
	}
  
	return result;
}


int send_msg(int dest_task_id, int port, int *msg) 
{
    struct port *p;
    int x, result;
	struct task *task;
    
    result = FAILURE;
    
	x = arch_cli(); /* enter critical block */
		
    if (0 <= dest_task_id && dest_task_id < MAX_TSK && TST_PTR(dest_task_id,tsk) && 0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{		
		task = (struct task*)GET_PTR(dest_task_id,tsk);

		if (task->state == ALIVE && task->open_ports[port] != NULL) 
		{
			if (VALIDATE_PTR(msg)) 
			{
				p = task->open_ports[port];
      
				if (p->mode != DISABLED && p->total < MAX_MSG_ON_PORT && !(p->mode == PERM_REQ && !getbit(p->perms, curr_task))) 
				{
				    if ((p->mode == UNRESTRICTED) || (curr_priv <= p->priv)) 
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
    
    arch_sti(x); /* exit critical block */
    
    return result;
}

int get_msg(int port, int *msg, int *id) 
{
    struct port *p;
	struct task *task;
    int x, result;
	
    result = FAILURE;
    
    if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{
		x = arch_cli(); /* enter critical block */
      
		task = (struct task*)GET_PTR(curr_task,tsk);
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
		
		arch_sti(x); /* exit critical block */
    }
    
    return result;
}

int get_msg_count(int port) 
{
	int x, res;
	struct task *task;

	x = arch_cli();

	task = (struct task*)GET_PTR(curr_task,tsk);
    if (0 > port || port >= MAX_TSK_OPEN_PORTS || task->open_ports[port] == NULL) 
		res = FAILURE;
    else
		res = task->open_ports[port]->total;

	arch_sti(x);

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
		{
			dst[i] = msg[i];
		}

		m->next = NULL;
		m->sender_id = from_task_id;
      
		/* Insert message at the end of the port */
      	if(p->first == NULL)
		{
			p->first = m;
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
    }
    
    return result;
}

void delete_task_ports(struct task *task) 
{
    int i, j, x;
    int old_free_first;

    x = arch_cli(); /* enter critical block */

    i = 0;

	while(i < MAX_TSK_OPEN_PORTS)
	{
		if(task->open_ports[i] != NULL) delete_port(task, task->open_ports[i]);
	}
        
    arch_sti(x); /* exit critical block */
}
