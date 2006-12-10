/*  
 *   Sartoris messaging subsystem header
 *   
 *   Copyright (C) 2002, 2003, 2004 
 *       Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#include "bitops.h"
#include "sartoris/kernel.h"

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#define MSG_LEN 4

struct message
{
	unsigned int sender_id;
	unsigned int data[MSG_LEN];
	struct message *next;
};

struct port
{
	struct message *first;        /* first message on port */
	struct message *last;         /* last message on port */
	unsigned int total;		      /* total messages on port */
	struct port *next;            /* next port assigned to this task */     
	enum usage_mode mode;
	unsigned int perms[BITMAP_SIZE(MAX_TSK)];
	int priv;
};

/* container functions */
struct port *create_port(struct task*);
void delete_port(struct task*, struct port*);
void delete_task_ports(struct task *);

/* port functions */
void empty(struct port *port);
int enqueue(int from_task_id, struct port *p, int *msg);
int dequeue(int *id, struct port *p, int *msg);

#endif
