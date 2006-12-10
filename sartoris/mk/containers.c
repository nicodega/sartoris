
#include "lib/containers.h"
#include "lib/indexing.h"
#include "lib/dynmem.h"

/* Static containers mem pool (this *must* be page aligned) */
unsigned char static_containers_mem [(CONT_STATIC_THR + CONT_STATIC_TSK + CONT_STATIC_SMO + CONT_STATIC_MSG + CONT_STATIC_PRT) * PG_SIZE] __attribute__ ((aligned (PG_SIZE))); 
struct _containers containers;

void cont_init(struct c_header *container, int type, int flags);

void init_containers()
{
	int i = 0;
	struct c_header *c;

	/* Initialize indexes */
	init_indexes();

	/* Initialize dynamic memory subsystem */
	dyn_init();

	/* Initialize static containers */
	containers.free[CONT_ALLOC_THR] = CONT_ALLOC_THR * CONT_THR_PER_CONT;
	containers.max_free[CONT_ALLOC_THR] = CONT_MAX_PREALLOC_THR;
	c = containers.first_free[CONT_ALLOC_THR] = (struct c_header*)static_containers_mem;
	for(; i < CONT_STATIC_THR; i++)
	{
		cont_init(c, CONT_ALLOC_THR, CONT_FLAGS_PREALLOC);
		c->next = ((i == CONT_STATIC_THR-1)? NULL : (struct c_header*)((unsigned int)c + PG_SIZE));
		c = (struct c_header*)((unsigned int)c + PG_SIZE);
	}

	containers.free[CONT_ALLOC_TSK] = CONT_ALLOC_TSK * CONT_TSK_PER_CONT;
	containers.max_free[CONT_ALLOC_TSK] = CONT_MAX_PREALLOC_TSK;
	c = containers.first_free[CONT_ALLOC_TSK] = (struct c_header*)&static_containers_mem[i * PG_SIZE];
	for(; i < CONT_STATIC_TSK; i++)
	{
		cont_init(c, CONT_ALLOC_TSK, CONT_FLAGS_PREALLOC);
		c->next = ((i == CONT_STATIC_TSK-1)? NULL : (struct c_header*)((unsigned int)c + PG_SIZE));
		c = (struct c_header*)((unsigned int)c + PG_SIZE);
	}

	containers.free[CONT_ALLOC_SMO] = CONT_ALLOC_SMO * CONT_SMO_PER_CONT;
	containers.max_free[CONT_ALLOC_SMO] = CONT_MAX_PREALLOC_SMO;
	c = containers.first_free[CONT_ALLOC_SMO] = (struct c_header*)&static_containers_mem[i * PG_SIZE];
	for(; i < CONT_STATIC_SMO; i++)
	{
		cont_init(c, CONT_ALLOC_SMO, CONT_FLAGS_PREALLOC);
		c->next = ((i == CONT_STATIC_SMO-1)? NULL : (struct c_header*)((unsigned int)c + PG_SIZE));
		c = (struct c_header*)((unsigned int)c + PG_SIZE);
	}

	containers.free[CONT_ALLOC_MSG] = CONT_ALLOC_MSG * CONT_MSG_PER_CONT;
	containers.max_free[CONT_ALLOC_MSG] = CONT_MAX_PREALLOC_MSG;
	c = containers.first_free[CONT_ALLOC_MSG] = (struct c_header*)&static_containers_mem[i * PG_SIZE];
	for(; i < CONT_STATIC_MSG; i++)
	{
		cont_init(c, CONT_ALLOC_MSG, CONT_FLAGS_PREALLOC);
		c->next = ((i == CONT_STATIC_MSG-1)? NULL : (struct c_header*)((unsigned int)c + PG_SIZE));
		c = (struct c_header*)((unsigned int)c + PG_SIZE);
	}

	containers.free[CONT_ALLOC_PRT] = CONT_ALLOC_PRT * CONT_PRT_PER_CONT;
	containers.max_free[CONT_ALLOC_PRT] = CONT_MAX_PREALLOC_PRT;
	c = containers.first_free[CONT_ALLOC_PRT] = (struct c_header*)&static_containers_mem[i * 0x1000];
	for(; i < CONT_STATIC_PRT; i++)
	{
		cont_init(c, CONT_ALLOC_PRT, CONT_FLAGS_PREALLOC);
		c->next = ((i == CONT_STATIC_PRT-1)? NULL : (struct c_header*)((unsigned int)c + 0x1000));
		c = (struct c_header*)((unsigned int)c + 0x1000);
	}
}

/*
Initializes a container with all free structures
*/
void cont_init(struct c_header *container, int type, int flags)
{
	int i = 0;
	
	container->first_free = (void*)((unsigned int)container + sizeof(struct c_header));
	container->flags = flags;
	container->next = NULL;

	switch(type)
	{
		case CONT_ALLOC_THR:
			container->free = CONT_THR_PER_CONT;
			struct c_thread_unit *thr = (struct c_thread_unit*)container->first_free;
			for(i = 0; i < container->free; i++)
			{
				thr->thread.next_free = (struct c_thread_unit *)((unsigned int)thr + sizeof(struct c_thread_unit));
				thr = thr->thread.next_free;
			}
			thr->thread.next_free = NULL;
			break;
		case CONT_ALLOC_TSK:
			container->free = CONT_TSK_PER_CONT;
			struct c_task_unit *tsk = (struct c_task_unit*)container->first_free;
			for(i = 0; i < container->free; i++)
			{
				tsk->task.next_free = (struct c_task_unit*)((unsigned int)tsk + sizeof(struct task));
				tsk = tsk->task.next_free;
			}
			tsk->task.next_free = NULL;
			break;
		case CONT_ALLOC_SMO:
			container->free = CONT_SMO_PER_CONT;
			struct smo *smo = (struct smo*)container->first_free;
			for(i = 0; i < container->free; i++)
			{
				smo->next = (struct smo *)((unsigned int)smo + sizeof(struct smo));
				smo = smo->next;
			}
			smo->next = NULL;
			break;
		case CONT_ALLOC_MSG:
			container->free = CONT_MSG_PER_CONT;
			struct message *msg = (struct message*)container->first_free;
			for(i = 0; i < container->free; i++)
			{
				msg->next = (struct message *)((unsigned int)msg + sizeof(struct message));
				msg = msg->next;
			}
			msg->next = NULL;
			break;
		case CONT_ALLOC_PRT:
			container->free = CONT_PRT_PER_CONT;
			struct port *prt = (struct port*)container->first_free;
			for(i = 0; i < container->free; i++)
			{
				prt->next = (struct port *)((unsigned int)prt + sizeof(struct port));
				prt = prt->next;
			}
			prt->next = NULL;
			break;
		default:
			break;
	}
}

/*
Given a structure type, this function will return an available one.
If _DYNAMIC_ is defined, when no more containers are available for
the given type, we will request memory from OS.
*/
void *csalloc(int type)
{
	struct c_header *c = containers.first_free[type];
	
	/* Find a container for this type with free entries */
	if(c == NULL)
	{
#ifdef _DYNAMIC_
		containers.first_free[type] = (struct c_header *)dyn_alloc_page(CONT_ALLOC2DYN(type));

		if(containers.first_free[type] == NULL) return NULL;

		/* Initialize container */
		cont_init(containers.first_free[type], type, CONT_FLAGS_NONE);

		switch(type)
		{
			case CONT_ALLOC_THR:
				containers.free[type] += CONT_THR_PER_CONT;
				break;
			case CONT_ALLOC_TSK:
				containers.free[type] += CONT_TSK_PER_CONT;
				break;
			case CONT_ALLOC_SMO:
				containers.free[type] += CONT_SMO_PER_CONT;
				break;
			case CONT_ALLOC_MSG:
				containers.free[type] += CONT_MSG_PER_CONT;
				break;
			case CONT_ALLOC_PRT:
				containers.free[type] += CONT_THR_PRT_CONT;
				break;
			default:
				break;
		}
#else
		return NULL;
#endif
	}

	/* Get a free entry on this container */
	void *free = c->first_free;

	/* Fix free list */
	switch(type)
	{
		case CONT_ALLOC_THR:
			c->first_free = ((struct c_thread_unit*)c->first_free)->thread.next_free;
			break;
		case CONT_ALLOC_TSK:
			c->first_free = ((struct c_task_unit*)c->first_free)->task.next_free;
			break;
		case CONT_ALLOC_SMO:
			c->first_free = ((struct smo*)c->first_free)->next;
			break;
		case CONT_ALLOC_MSG:
			c->first_free = ((struct message*)c->first_free)->next;
			break;
		case CONT_ALLOC_PRT:
			c->first_free = ((struct port*)c->first_free)->next;
			break;
		default:
			break;
	}

	/* fix free count */
	c->free--;
	containers.free[type]--;

	/* 
	If free count is 0, remove this 
	container from the free list
	*/
	if(c->free == 0)
		containers.first_free[type] = c->next;

	return free;
}

/*
Frees a structure allocated with salloc. Type of the structure
must be provided.
*/
void csfree(void *ptr, int type)
{	
	struct c_header *c = (struct c_header *)CONT_HEADER_ADDR(ptr);
	int entries = 0;
	
	/* Add current structure to the container free list */
	switch(type)
	{
		case CONT_ALLOC_THR:
			((struct c_thread_unit*)ptr)->thread.next_free = c->first_free;
			c->first_free = ((struct c_thread_unit*)ptr);
			entries = CONT_THR_PER_CONT;
			break;
		case CONT_ALLOC_TSK:
			((struct c_task_unit*)ptr)->task.next_free = c->first_free;
			c->first_free = ((struct c_task_unit*)ptr);
			entries = CONT_TSK_PER_CONT;
			break;
		case CONT_ALLOC_SMO:
			((struct smo*)ptr)->next = c->first_free;
			c->first_free = ((struct smo*)ptr);
			entries = CONT_SMO_PER_CONT;
			break;
		case CONT_ALLOC_MSG:
			((struct message*)ptr)->next = c->first_free;
			c->first_free = ((struct message*)ptr);
			entries = CONT_MSG_PER_CONT;
			break;
		case CONT_ALLOC_PRT:
			((struct port*)ptr)->next = c->first_free;
			c->first_free = ((struct port*)ptr);
			entries = CONT_PRT_PER_CONT;
			break;
		default:
			break;
	}

	/* Increment free count */
	c->free++;

	containers.free[type]++;

	/* If free count is 1, add this container to the free list */
	if(c->free == 1)
	{		
		c->next = containers.first_free[type];
		containers.first_free[type] = c;
	}

	/* Will we return this page to the OS? */	
	if(!(c->flags & CONT_FLAGS_PREALLOC) && c->free == entries)
	{
		/*
		We will free the page, if total free structures
		of this type, are above a define
		*/
		if(containers.free[type] > containers.max_free[type])
		{
#ifdef _DYNAMIC_
			// IMPLEMENT //
#endif
			return;
		}			
	}
}



