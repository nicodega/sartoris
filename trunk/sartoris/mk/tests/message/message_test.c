
/*  
 *   Sartoris 0.5 messaging subsystem implementation
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

//#include <sartoris/kernel.h>
//#include <sartoris/message.h>
//#include <sartoris/cpu-arch.h>

#include "../../include/sartoris/message.h" 

void init_msg(struct msg_container *mc) {
  int i,j;
 
  mc->free_first = mc->ports_free_first = 0;

  /* init message repository */
  i=0;
  while (i<MAX_MSG-1) { mc->messages[i].next = ++i; }
  
  mc->messages[MAX_MSG-1].next = -1;
  
  /* initialize ports */
  for(i=0; i<MAX_OPEN_PORTS-1; i++) { 
		mc->ports[i].first = mc->ports[i].last = -1;
		mc->ports[i].total = 0;
		mc->ports[i].next = i+1;      /* set next on the ports list */
  }

  mc->ports[MAX_OPEN_PORTS-1].first = mc->ports[MAX_OPEN_PORTS-1].last = -1;
  mc->ports[MAX_OPEN_PORTS-1].total = 0;
  mc->ports[MAX_OPEN_PORTS-1].next = -1;

  /* initialize first_port array */

  for(j=0; j<MAX_TSK; j++) {
    mc->task_first_port[j] = -1;	/* the task has no ports assigned */
  }
  
}


int create_port(struct msg_container* mc, int task) {
  int new, next;
  int x;
  int i;
  
  //x = arch_cli();
  
  new = mc->ports_free_first;
  
  if(new != -1) { /* we have a free port! */
    mc->ports_free_first = mc->ports[new].next;
    
    mc->ports[new].next = next = mc->task_first_port[task];
    mc->ports[new].prev = -1;
	
    if (next != -1) {
      mc->ports[next].prev = new;
    }
    
    mc->task_first_port[task] = new;
  }

  //arch_sti(x);
  return new;
  
}

int delete_port(struct msg_container *mc, int task, int port) {
  int x;
  int prev, next;
  
  //x = arch_cli();

  empty(mc, port); /* first empty the port */

  prev = mc->ports[port].prev;
  next = mc->ports[port].next;
    
  if (mc->task_first_port[task] == port) {
    mc->task_first_port[task] = mc->ports[port].next;
  }
    
  if (prev != -1) { mc->ports[prev].next = next; }
  if (next != -1) { mc->ports[next].prev = prev; }
    
  mc->ports[port].next = mc->ports_free_first;
  mc->ports_free_first = port;
  
  //arch_sti(x);
  return 0;
}


int enqueue(struct msg_container *mc, int id, int port, int *msg) {
  struct port *p;
  unsigned int free, old_last;
  int *dst;
  int i, x;
  
  //x = arch_cli();
  
  if (port >= MAX_TSK_OPEN_PORTS || mc->free_first == -1 ) { 
    //arch_sti(x); 
    return -1; 
  }
  
  p = &mc->ports[port];
  
  free = mc->free_first;  /* this is the first free _message_ */
  old_last = p->last;
  
  p->last = free;
  
  mc->free_first = mc->messages[free].next;
  
  mc->messages[free].next = -1;
  mc->messages[free].sender_id = id;
  dst = mc->messages[free].data;
  for(i=0; i<MSG_LEN; i++) {dst[i] = msg[i]; }
  
  if ( !(p->total++) ) { 
    p->first = free; 
  } else {
    mc->messages[old_last].next = free;
  }
  
  //arch_sti(x);
  return 0;
}
  
int dequeue(struct msg_container *mc, int *id, int port, int *msg){
  struct port *p;
  int i, x;
  unsigned int old_first;
  int *src;
  
  //x = arch_cli();

  if ( port >= MAX_TSK_OPEN_PORTS ) { 
    //arch_sti(x);
    return -1; 
  }

  p = &mc->ports[port];

  old_first = p->first;
  
  if ( old_first == -1 ) { 
    //arch_sti(x); 
    return -1; 
  }
  
  p->first = mc->messages[old_first].next;
  mc->messages[old_first].next = mc->free_first;
  mc->free_first = old_first;
  
  *id = mc->messages[old_first].sender_id;
  src = mc->messages[old_first].data;
  for(i=0; i<MSG_LEN; i++) { msg[i] = src[i]; }
  
  if ( !(--p->total) ) { p->last = -1; }
  
  //arch_sti(x);
  return 0;
}

int empty(struct msg_container *mc, int port){

  int i, j, x;
  int old_free_first;
  
  //x = arch_cli();
  
  if ( port >= MAX_TSK_OPEN_PORTS ) { 
    //arch_sti(x);
    return -1; 
  }
  
  i = mc->ports[port].first;
  mc->ports[port].first = -1;
  mc->ports[port].last = -1;
  mc->ports[port].total = 0;
  
  if (i != -1) {
    old_free_first = mc->free_first;
    mc->free_first = i;
    
    while(i != -1) {
      j=i;
      i=mc->messages[i].next;
    }
    mc->messages[j].next = old_free_first;
  }
  
  //arch_sti(x);

}

void delete_task_ports(struct msg_container *mc, int task_id) {
	
  int i, j, x;
  int old_free_first;
  
  //x = arch_cli();
  
  i = mc->task_first_port[task_id];
  mc->task_first_port[task_id] = -1;
  
  if (i != -1) {
    old_free_first=mc->ports_free_first;
    mc->ports_free_first = i;
    
    while(i != -1) {
      empty(mc, i);   /* empty the port we are about to delete */
      j=i;
      i=mc->ports[i].next;
    }
    mc->ports[j].next = old_free_first;
  }
  
  //arch_sti(x);
  
}

