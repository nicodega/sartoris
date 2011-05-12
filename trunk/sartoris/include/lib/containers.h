
#include "sartoris/kernel.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "kernel-arch.h"

#ifndef _CONTAINERSH_
#define _CONTAINERSH_

#ifndef PAD
#define PAD(a,b) (a + (b - (a % b)))
#endif

/*
Definition of different per page containers.
*/
#define CONT_FLAGS_NONE       0x0		// No flags for this container
#define CONT_FLAGS_PREALLOC   0x1		// Container is static

#define CONT_AVAILABLE    (PG_SIZE - sizeof(struct c_header))	// Size remaining on a container

/* Entries for structures for each container */
#define CONT_THR_PER_CONT     (CONT_AVAILABLE / sizeof(struct c_thread_unit))
#define CONT_TSK_PER_CONT     (CONT_AVAILABLE / sizeof(struct c_task_unit))
#define CONT_SMO_PER_CONT     (CONT_AVAILABLE / sizeof(struct smo))
#define CONT_MSG_PER_CONT     (CONT_AVAILABLE / sizeof(struct message))
#define CONT_PRT_PER_CONT     (CONT_AVAILABLE / sizeof(struct port))

#define STATIC_THR 128
#define STATIC_TSK 64
#define STATIC_SMO 1024
#define STATIC_MSG 1024
#define STATIC_PRT (MAX_TSK_OPEN_PORTS*STATIC_TSK)

/* Next defines are expressed on container ammounts */
#define CONT_STATIC_THR      (PAD(STATIC_THR, CONT_THR_PER_CONT) / CONT_THR_PER_CONT)
#define CONT_STATIC_TSK      (PAD(STATIC_TSK, CONT_TSK_PER_CONT) / CONT_TSK_PER_CONT)
#define CONT_STATIC_SMO      (PAD(STATIC_SMO, CONT_SMO_PER_CONT) / CONT_SMO_PER_CONT)
#define CONT_STATIC_MSG      (PAD(STATIC_MSG, CONT_MSG_PER_CONT) / CONT_MSG_PER_CONT)
#define CONT_STATIC_PRT      (PAD(STATIC_PRT, CONT_PRT_PER_CONT) / CONT_PRT_PER_CONT)

/* 
Maximum structures to keep before returning a container.
*/
#define CONT_MAX_PREALLOC_THR     (CONT_STATIC_THR * CONT_THR_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_TSK     (CONT_STATIC_TSK * CONT_TSK_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_SMO     (CONT_STATIC_SMO * CONT_SMO_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_MSG     (CONT_STATIC_MSG * CONT_MSG_PER_CONT * 2 / 3)
#define CONT_MAX_PREALLOC_PRT     (CONT_STATIC_PRT * CONT_PRT_PER_CONT * 2 / 3)

#define CONT_STATIC_CONTAINERS  (CONT_STATIC_THR + CONT_STATIC_TSK + CONT_STATIC_SMO + CONT_STATIC_MSG + CONT_STATIC_PRT)

/* Standard container header */
struct c_header // 16 bytes
{
	short flags;			// Container flags
	short free;             // free structures on this container
	struct c_header *next;  // next container
	struct c_header *prev;  // prev container
	void *first_free;	    // first free structure on container (NULL if none)
};

/* 
Thread container.
For each thread we will hold it's information and state (state structure
size will be defined on ARCH dependant section)
*/
struct c_thread_unit
{
	/* The thread structure */
	struct thread thread;
	/* Pointer to next free thread. Used only when thread is free. */
	struct c_thread_unit *next_free;
	/* Arch dependant information structure buffer */
	unsigned char arch[ARCH_STATE_SIZE];
};

#define CONT_THR_STATE_PTR(thr) ((void*)&((struct c_thread_unit*)thr)->arch)
#define CONTR_THR_FROM_THRSTATE(state) ((struct thread*)((unsigned int)state - sizeof(struct thread) - 4))

#define PADDING(s,count) (PG_SIZE - sizeof(struct s) * count - sizeof(struct c_header))

struct c_thread
{
	struct c_header header;
	struct c_thread_unit threads[CONT_THR_PER_CONT];
	unsigned char padding[PADDING(c_thread_unit,CONT_THR_PER_CONT)];
};

/* 
Tasks container.
*/
struct c_task_unit
{
	/* The task structure */
	struct task   task;
	/* Pointer to next free task. Used only when task is free. */
	struct c_task_unit *next_free;		
	/* Arch dependant information structure buffer */
	unsigned char arch[ARCH_TASK_SIZE];
};

#define CONT_TSK_ARCH_PTR(tsk) ((void*)&((struct c_task_unit*)tsk)->arch)

struct c_task
{
	struct c_header     header;
	struct c_task_unit  tasks[CONT_TSK_PER_CONT];
	unsigned char       padding[PADDING(c_task_unit,CONT_TSK_PER_CONT)];
};


/* SMO container */
struct c_smo
{
	struct c_header		header;
	struct smo			smos[CONT_SMO_PER_CONT];
	unsigned char		padding[PADDING(smo,CONT_SMO_PER_CONT)];
};

/* Message container */
struct c_message
{
	struct c_header		header;
	struct message		msgs[CONT_MSG_PER_CONT];
	unsigned char		padding[PADDING(message,CONT_MSG_PER_CONT)];
};

/* Ports */
struct c_port
{
	struct c_header		header;
	struct port			ports[CONT_PRT_PER_CONT];
	unsigned char		padding[PADDING(port,CONT_PRT_PER_CONT)];
};

/* 
	Given a structure address, get it's container header address .
	Since we required all structures to be aligned on page boundaries,
	we can get container header address by finding the first lower page boundary.
*/
#define CONT_HEADER_ADDR(objaddr) ((unsigned int)objaddr - ((unsigned int)objaddr % PG_SIZE))

/* Container types */
#define CONT_ALLOC_THR   0
#define CONT_ALLOC_TSK   1
#define CONT_ALLOC_SMO   2
#define CONT_ALLOC_PRT   3
#define CONT_ALLOC_MSG   4

#define CONT_TYPES       5				// Ammount of container types
#define CONT_ALLOC2DYN(type) (type)		// Transform Alloc type to dynamic memory (containers) type.

struct _containers
{
	struct c_header *first_free[CONT_TYPES];	// First free container
	int free[CONT_TYPES];					    // Total structures free on all containers
	int max_free[CONT_TYPES];				    // Number of structs we will leave until starting to free containers
};

/* Container allocation function definition. */
void *csalloc(int type);
void csfree(void *ptr,int type);
void init_containers();

#endif 

