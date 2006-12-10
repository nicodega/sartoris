
#include "sartoris/kernel.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "kernel-arch.h"

#ifndef _CONTAINERSH_
#define _CONTAINERSH_

/*
Definition of different per page containers.
*/
#define CONT_FLAGS_NONE       0x0
#define CONT_FLAGS_PREALLOC   0x1

#define CONT_AVAILABLE    (PG_SIZE - sizeof(struct c_header))

/* Entries for structures for each container */
#define CONT_THR_PER_CONT     (CONT_AVAILABLE / sizeof(struct c_thread_unit) )
#define CONT_TSK_PER_CONT     (CONT_AVAILABLE / sizeof(struct c_task_unit)   )
#define CONT_SMO_PER_CONT     (CONT_AVAILABLE / sizeof(struct smo) )
#define CONT_MSG_PER_CONT     (CONT_AVAILABLE / sizeof(struct message) )
#define CONT_PRT_PER_CONT     (CONT_AVAILABLE / sizeof(struct port) )

/* 
Maximum prealloc structures to keep before returning
a container.
*/
#define CONT_MAX_PREALLOC_THR     (CONT_STATIC_THR * CONT_THR_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_TSK     (CONT_STATIC_TSK * CONT_TSK_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_SMO     (CONT_STATIC_SMO * CONT_SMO_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_MSG     (CONT_STATIC_MSG * CONT_MSG_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_PRT     (CONT_STATIC_PRT * CONT_PRT_PER_CONT * 2 / 3)

/* Next defines are expressed on container ammounts */
#define CONT_STATIC_THR      (128 / CONT_THR_PER_CONT)
#define CONT_STATIC_TSK      (64 / CONT_TSK_PER_CONT)
#define CONT_STATIC_SMO      (1024 / CONT_SMO_PER_CONT)
#define CONT_STATIC_MSG      (1024 / CONT_MSG_PER_CONT)
#define CONT_STATIC_PRT      ((MAX_TSK_OPEN_PORTS*64) / CONT_SMO_PER_CONT)

/* Standard container header */
struct c_header
{
	short flags;
	short free;             // free structures on this container
	struct c_header *next;  // next container
	void *first_free;	    // first free structure on container
};

/* 
Thread container.
For each thread we will hold it's information and state (state structure
size will be defined on ARCH dependant section)
*/
#define CONT_THR_STATE_PTR(thr) ((void*)((unsigned int)thr + sizeof(struct thread)))

struct c_thread_unit
{
	/* The thread structure */
	struct thread thread;
	/* Arch dependant information structure buffer */
	unsigned char arch_buff[ARCH_STATE_SIZE];
};

struct c_thread
{
	struct c_header header;
	struct c_thread_unit threads[CONT_THR_PER_CONT];
	unsigned char   padding[PG_SIZE - CONT_THR_PER_CONT - sizeof(struct c_header)];
};

/* 
Tasks container.
*/
#define CONT_TSK_ARCH_PTR(thr) ((void*)((unsigned int)thr + sizeof(struct thread)))

struct c_task_unit
{
	/* The task structure */
	struct task   task;
	/* Arch dependant information structure buffer */
	unsigned char arch_buff[ARCH_TASK_SIZE];
};

struct c_task
{
	struct c_header     header;
	struct c_task_unit  tasks[CONT_TSK_PER_CONT];
	unsigned char       padding[PG_SIZE - CONT_TSK_PER_CONT - sizeof(struct c_header)];
};


/* SMO container */
struct c_smo
{
	struct c_header header;
	struct smo smos[CONT_SMO_PER_CONT];
	unsigned char   padding[PG_SIZE - CONT_SMO_PER_CONT - sizeof(struct c_header)];
};

/* Message container */
struct c_message
{
	struct c_header header;
	struct message msgs[CONT_MSG_PER_CONT];
	unsigned char   padding[PG_SIZE - CONT_MSG_PER_CONT - sizeof(struct c_header)];
};

/* Ports */
struct c_port
{
	struct c_header header;
	struct port ports[CONT_PRT_PER_CONT];
	unsigned char   padding[PG_SIZE - CONT_PRT_PER_CONT - sizeof(struct c_header)];
};

/* 
	Given a structure address, get it's container header address .
	Since we required all structures to be aligned on page boundaries,
	we can get container header address by finding the first lower page boundary.
*/
#define CONT_HEADER_ADDR(objaddr) ((unsigned int)objaddr - ((unsigned int)objaddr % PG_SIZE))

#define CONT_ALLOC_THR   0
#define CONT_ALLOC_TSK   1
#define CONT_ALLOC_SMO   2
#define CONT_ALLOC_PRT   3
#define CONT_ALLOC_MSG   4

#define CONT_TYPES       5
#define CONT_ALLOC2DYN(type) (type)

struct _containers
{
	struct c_header *first_free[CONT_ALLOC_MSG];	// first free container
	int free[CONT_ALLOC_MSG];					// total structures free on all containers
	int max_free[CONT_ALLOC_MSG];				// number of structs we will leave until starting to free containers
};

void *csalloc(int type);
void csfree(void *ptr,int type);
void init_containers();

#endif 

