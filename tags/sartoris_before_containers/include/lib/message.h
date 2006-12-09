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

#define MAX_MSG   1024                /* system-wide */
#define MAX_OPEN_PORTS (32*MAX_TSK)   /* system-wide */
#define MAX_TSK_OPEN_PORTS 32
#define MAX_MSG_ON_PORT	32 

#define MSG_LEN 4

struct message
{
  unsigned int sender_id;
  unsigned int data[MSG_LEN];
  unsigned int next;
};

struct port
{
  unsigned int first;           /* first message on port */
  unsigned int last;		/* last message on port */
  unsigned int total;		/* total messages on port */
  unsigned int prev;            /* previous port assigned to this task */
  unsigned int next;            /* next port assigned to this task */
  unsigned int perm[BITMAP_SIZE(MAX_TSK)];	     
  enum usage_mode mode;
  int priv;
};

struct msg_container
{
  int task_first_port[MAX_TSK];	/* first assigned port for any given task */
  struct port ports[MAX_OPEN_PORTS];		 /* port buffer */
  struct message messages[MAX_MSG];		 /* message buffer */
  unsigned int free_first;             /* start of free messages list */
  unsigned int ports_free_first;       /* start of free ports list */
};

/* container functions */
void init_msg(struct msg_container *mc);
int create_port(struct msg_container *mc, int task);
void delete_port(struct msg_container *mc, int task, int port);
void delete_task_ports(struct msg_container *mc, int task_id);

/* port functions */
void empty(struct msg_container *mc, int port);
int enqueue(struct msg_container *mc, int id, int port, int *msg);
int dequeue(struct msg_container *mc, int *id, int port, int *msg);

#endif
