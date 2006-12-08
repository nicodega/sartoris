
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

/* message subsystem implementation */

/* these are the proper system calls: */
int open_port(int port, int priv, enum usage_mode mode) 
{
    int i, new;
    int x, result;
    
    result = FAILURE;

    if (0 <= mode && mode < MAX_USAGE_MODE) 
	{
		if (0 <= port && port <= MAX_TSK_OPEN_PORTS) 
		{
	
			x = arch_cli(); /* enter critical block */
	
			if (open_ports[curr_task][port] == UNUSED) 
			{
	   			new = create_port(&msgc, curr_task); /* look up a free port */
	  
				if (new != FAILURE) 
				{
					/* check if port was found */
					for (i = 0; i < (BITMAP_SIZE(MAX_TSK)); i++) 
					{
						msgc.ports[new].perm[i] = 0;
					}
	    
					msgc.ports[new].mode = mode;
					msgc.ports[new].priv = priv;
	    
					open_ports[curr_task][port] = new;
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
    
    result = FAILURE;
    
    if (0 <= port && port <= MAX_TSK_OPEN_PORTS) 
	{
		x = arch_cli(); /* enter critical block */
      
		if (open_ports[curr_task][port] >= 0) 
		{
			result = SUCCESS;
	
			p = open_ports[curr_task][port];
			delete_port(&msgc, curr_task, p);
			open_ports[curr_task][port] = UNUSED;

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
    
    result = FAILURE;
  
    if (0 <= mode && mode < MAX_USAGE_MODE) 
	{
		x = arch_cli(); /* enter critical block */
		
		p = open_ports[curr_task][port];

		if (p >= 0) 
		{
			result = SUCCESS;
			msgc.ports[p].priv = priv;
			msgc.ports[p].mode = mode;
		}
      
		arch_sti(x); /* exit critical block */
    }
    
    return result;
}

int set_port_perm(int port, int task, int perm) 
{
	int p, x, result;
  
	result = FAILURE;
  
	if (0 <= task && task < MAX_TSK && 0 <= port && port < MAX_TSK_OPEN_PORTS && !(perm & ~(1))) 
	{
	    x = arch_cli(); /* enter critical block */
    
		p = open_ports[curr_task][port];
    
		if ( p >= 0) 
		{
			result = SUCCESS;
			setbit(msgc.ports[p].perm, task, perm);
		}
    
		arch_sti(x); /* exit critical block */
	}
  
	return result;
}


int send_msg(int dest_task_id, int port, int *msg) 
{
    struct port *p;
    int x, result;
    
    result = FAILURE;
    
	x = arch_cli(); /* enter critical block */
		
    if (0 <= dest_task_id && dest_task_id < MAX_TSK && 0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{		
		if (tasks[dest_task_id].state == ALIVE && open_ports[dest_task_id][port] >= 0) 
		{
			if (VALIDATE_PTR(msg)) 
			{
				p = &msgc.ports[open_ports[dest_task_id][port]];
      
				if (p->mode != DISABLED && !(p->mode == PERM_REQ && !getbit(p->perm, curr_task))) 
				{
				    if ((p->mode == UNRESTRICTED) || (curr_priv <= p->priv)) 
					{
						result = enqueue(&msgc, curr_task, open_ports[dest_task_id][port], (int *) MAKE_KRN_PTR(msg));
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
    int p, x, result;
    
    result = FAILURE;
    
    if (0 <= port && port < MAX_TSK_OPEN_PORTS) 
	{
		x = arch_cli(); /* enter critical block */
      
		p = open_ports[curr_task][port];
      
		if (p >= 0) 
		{
			if (VALIDATE_PTR(msg) && VALIDATE_PTR(id)) 
			{
				result = dequeue(&msgc, (int *) MAKE_KRN_PTR(id), p, (int *) MAKE_KRN_PTR(msg));
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

	x = arch_cli();

    if (0 > port || port >= MAX_TSK_OPEN_PORTS || open_ports[curr_task][port] == -1) 
		res = -1;
    else
		res = msgc.ports[open_ports[curr_task][port]].total;

	arch_sti(x);

	return res;
}


/* the following functions implement the data structures used above: */


/* init_msg is called during kernel initialization, there are no interrupts */

void init_msg(struct msg_container *mc) 
{
    int i, j;

    mc->free_first = mc->ports_free_first = 0;

    /* init message repository */
    i = 0;
    while (i < MAX_MSG - 1) 
	{
		mc->messages[i].next = ++i;
    }

    mc->messages[MAX_MSG - 1].next = -1;

    /* initialize ports */
    for (i = 0; i < MAX_OPEN_PORTS; i++) 
	{
		mc->ports[i].first = mc->ports[i].last = -1;
		mc->ports[i].total = 0;
		mc->ports[i].next = i + 1;	/* set next on the ports list */
    }

    mc->ports[MAX_OPEN_PORTS - 1].next = -1;

    /* initialize first_port array */

    for (j = 0; j < MAX_TSK; j++) 
	{
		mc->task_first_port[j] = -1;	/* the task has no ports assigned */
    }
}



/* this function is called within a critical block */
/* task is assumed to be in range */
int create_port(struct msg_container *mc, int task) 
{
    int new, old_first;
    int i;

    new = mc->ports_free_first;

    if (new != -1) 
	{		/* we have a free port! */
		mc->ports_free_first = mc->ports[new].next;

		mc->ports[new].next = old_first = mc->task_first_port[task];
		mc->ports[new].prev = -1;

		if (old_first != -1) 
		{
			mc->ports[old_first].prev = new;
		}

		mc->task_first_port[task] = new;
    }
    
    return new;
}

/* 1. this function is called within a critical block */
/* 2. task is assumed to be in range                  */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open (i.e. not in the free 
      ports list) */
void delete_port(struct msg_container *mc, int task, int port) 
{
	int prev, next;

    empty(mc, port);		/* first empty the port, move all   */
                            /* its queued messages to free list */
    
    prev = mc->ports[port].prev;
    next = mc->ports[port].next;

    if (mc->task_first_port[task] == port) 
	{
		mc->task_first_port[task] = next;
    }

    if (prev != -1) 
	{
		mc->ports[prev].next = next;
    }
    if (next != -1) 
	{
		mc->ports[next].prev = prev;
    }

    mc->ports[port].next = mc->ports_free_first;
    mc->ports_free_first = port;
    
    return;
}

/* 1. this function is called within a critical block */
/* 2. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 3. port should be open & good (i.e. its queued 
      messages should NOT be in the free list)        */
void empty(struct msg_container *mc, int port) 
{
    int i, j;
    int old_free_first;
      
    i = mc->ports[port].first;
    mc->ports[port].first = -1;
    mc->ports[port].last = -1;
    mc->ports[port].total = 0;
      
    if (i != -1) 
	{
		old_free_first = mc->free_first;
		mc->free_first = i;
      
		while (i != -1) 
		{
			j = i;
			i = mc->messages[i].next;
		}
		mc->messages[j].next = old_free_first;
    }
    
    return;
}

/* 1. this function is called within a critical block */
/* 2. from_task_id is assumed to be within range      */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open & good                      */
int enqueue(struct msg_container *mc, int from_task_id, int port, int *msg) 
{
    struct port *p;
    unsigned int free, old_last;
    int *dst;
    int i, result;
    
    result = FAILURE;
    
    if (mc->free_first >= 0) 
	{
		result = SUCCESS;
      
		p = &mc->ports[port];
      
		free = mc->free_first;	/* this is the first free _message_ */
		old_last = p->last;
      
		p->last = free;
      
		mc->free_first = mc->messages[free].next;
      
		mc->messages[free].next = -1;
		mc->messages[free].sender_id = from_task_id;
		dst = (int*)mc->messages[free].data;
		for (i = 0; i < MSG_LEN; i++) 
		{
			dst[i] = msg[i];
		}
      
		if (!(p->total++)) 
		{
			p->first = free;
		} 
		else 
		{
			mc->messages[old_last].next = free;
		}
    }
    
    return result;
}

/* 1. this function is called within a critical block */
/* 2. pointers are assumed to be good (id and msg)    */
/* 3. 0 <= port < MAX_OPEN_PORTS is assumed           */
/* 4. port should be open & good                      */
int dequeue(struct msg_container *mc, int *id, int port, int *msg) 
{
    struct port *p;
    int i, result;
    unsigned int old_first;
    int *src;
    
    result = FAILURE;
    
    p = &mc->ports[port];

    old_first = p->first;

    if (old_first >= 0) 
	{
		result = SUCCESS;
      
		p->first = mc->messages[old_first].next;
		mc->messages[old_first].next = mc->free_first;
		mc->free_first = old_first;
      
		*id = mc->messages[old_first].sender_id;

		src = (int*)mc->messages[old_first].data;
		for (i = 0; i < MSG_LEN; i++) 
		{
			msg[i] = src[i];
		}
      
		if (!(--p->total)) 
		{
			p->last = -1;
		}
    }
    
    return result;
}

void delete_task_ports(struct msg_container *mc, int task_id) 
{
    int i, j, x;
    int old_free_first;

    x = arch_cli(); /* enter critical block */

    i = mc->task_first_port[task_id];
    mc->task_first_port[task_id] = -1;

    if (i != -1) 
	{
		old_free_first = mc->ports_free_first;
		mc->ports_free_first = i;

		while (i != -1) 
		{
			empty(mc, i);	/* empty the port we are about to delete */
			j = i;
			i = mc->ports[i].next;
		}
		mc->ports[j].next = old_free_first;
    }

    arch_sti(x); /* exit critical block */
}
