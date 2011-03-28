
#include "lib/containers.h"
#include "lib/indexing.h"
#include "lib/dynmem.h"

/* Static containers mem pool (this *must* be page aligned) */
unsigned char static_containers_mem[CONT_STATIC_CONTAINERS * PG_SIZE] __attribute__ ((aligned(PG_SIZE))); 
struct _containers containers;

void cont_init(struct c_header *container, int type, int flags);

int init_container_type(int type, int static_elem_count, int static_containers, struct c_header* first_container, int i)
{
	struct c_header *c;
	int j;

	// set free elements for this type to all elements statically allocated
	containers.free[type] = static_elem_count;
	// set max free to the ammount of static elements on one container
	containers.max_free[type] = static_elem_count / static_containers; // We will keep one container in order to avoid initializing it again
	// first free container points to the first static container
	c = containers.first_free[type] = first_container;

	for(j = 0; j < static_containers; j++)
	{
		// Initialize the container as static
		cont_init(c, type, CONT_FLAGS_PREALLOC);
		c->prev = ((j == 0)? NULL : (struct c_header*)((unsigned int)c - PG_SIZE));
		c->next = ((j == static_containers - 1)? (struct c_header*)NULL : (struct c_header*)((unsigned int)c + PG_SIZE));
		c = (struct c_header*)((unsigned int)c + PG_SIZE);
		i++;
	}	
	return i;
}

void init_containers()
{
	/* Initialize static containers */
	int i = 0;

	i = init_container_type(CONT_ALLOC_THR, CONT_STATIC_THR * CONT_THR_PER_CONT, CONT_STATIC_THR, 
						(struct c_header*)static_containers_mem, 0);
	
	i = init_container_type(CONT_ALLOC_TSK, CONT_STATIC_TSK * CONT_TSK_PER_CONT, CONT_STATIC_TSK, 
						(struct c_header*)&static_containers_mem[i * PG_SIZE], i);
		
	i = init_container_type(CONT_ALLOC_SMO, CONT_STATIC_SMO * CONT_SMO_PER_CONT, CONT_STATIC_SMO, 
						(struct c_header*)&static_containers_mem[i * PG_SIZE], i);
	
	i = init_container_type(CONT_ALLOC_MSG, CONT_STATIC_MSG * CONT_MSG_PER_CONT, CONT_STATIC_MSG, 
						(struct c_header*)&static_containers_mem[i * PG_SIZE], i);
	
	init_container_type(CONT_ALLOC_PRT, CONT_STATIC_PRT * CONT_PRT_PER_CONT, CONT_STATIC_PRT, 
						(struct c_header*)&static_containers_mem[i * PG_SIZE], i);	
}

/*
Initializes a container with all free structures
*/
void cont_init(struct c_header *container, int type, int flags)
{
	int i = 0;
    void *ptr = NULL;
	
	container->first_free = (void*)((unsigned int)container + sizeof(struct c_header));
	container->flags = flags;
	container->next = container->prev = NULL;

	switch(type)
	{
		case CONT_ALLOC_THR:
			container->free = CONT_THR_PER_CONT;
			ptr = container->first_free;
			for(i = 1; i < container->free; i++)
			{
				((struct c_thread_unit *)ptr)->next_free = (struct c_thread_unit *)((unsigned int)ptr + sizeof(struct c_thread_unit));
				ptr = ((struct c_thread_unit *)ptr)->next_free;
			}
			((struct c_thread_unit *)ptr)->next_free = NULL;
			break;
		case CONT_ALLOC_TSK:
			container->free = CONT_TSK_PER_CONT;
			ptr = container->first_free;
			for(i = 1; i < container->free; i++)
			{
				((struct c_task_unit *)ptr)->next_free = (struct c_task_unit*)((unsigned int)ptr + sizeof(struct c_task_unit));
				ptr = ((struct c_task_unit *)ptr)->next_free;
			}
			((struct c_task_unit *)ptr)->next_free = NULL;
			break;
		case CONT_ALLOC_SMO:
			container->free = CONT_SMO_PER_CONT;
			ptr = container->first_free;
			for(i = 1; i < container->free; i++)
			{
				((struct smo*)ptr)->next = (struct smo *)((unsigned int)ptr + sizeof(struct smo));
				ptr = ((struct smo*)ptr)->next;
			}
			((struct smo*)ptr)->next = NULL;
			break;
		case CONT_ALLOC_MSG:
			container->free = CONT_MSG_PER_CONT;
			ptr = container->first_free;
			for(i = 1; i < container->free; i++)
			{
				((struct message*)ptr)->next = (struct message *)((unsigned int)ptr + sizeof(struct message));
				ptr = ((struct message*)ptr)->next;
			}
			((struct message*)ptr)->next = NULL;
			break;
		case CONT_ALLOC_PRT:
			container->free = CONT_PRT_PER_CONT;
			ptr = container->first_free;
			for(i = 1; i < container->free; i++)
			{
				((struct port*)ptr)->next = (struct port *)((unsigned int)ptr + sizeof(struct port));
				ptr = ((struct port*)ptr)->next;
			}
			((struct port*)ptr)->next = NULL;
			break;
		default:
			break;
	}
}

/*
Given a structure type, this function will return an available one.
When no more containers are available for the given type, we will request memory from OS.
This function might break atomicity.
*/
void *csalloc(int type)
{
	struct c_header *c = containers.first_free[type];
	
	/* Find a container for this type with free entries */
	if(c == NULL)
	{		
		bprintf(12, "mk/containers.c: ASKING FOR PAGE type: %i \n", type);
	    /* No free container available */
		c = containers.first_free[type] = (struct c_header *)dyn_alloc_page(CONT_ALLOC2DYN(type));

		if(containers.first_free[type] == NULL) 
			return NULL;
		
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
				containers.free[type] += CONT_PRT_PER_CONT;
				break;
			default:
				break;
		}
	}

	/* Get a free entry on this container */
	void *free = c->first_free;

	/* Fix free list */
	switch(type)
	{
		case CONT_ALLOC_THR:
			c->first_free = ((struct c_thread_unit*)c->first_free)->next_free;
			break;
		case CONT_ALLOC_TSK:
			c->first_free = ((struct c_task_unit*)c->first_free)->next_free;
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
	{
		containers.first_free[type] = c->next;
		if(c->next != NULL)
			c->next->prev = NULL;		
	}

	return free;
}

/*
Frees a structure allocated with csalloc. Type of the structure
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
			((struct c_thread_unit*)ptr)->next_free = (struct c_thread_unit*)c->first_free;
			c->first_free = ((struct c_thread_unit*)ptr);
			entries = CONT_THR_PER_CONT;
			break;
		case CONT_ALLOC_TSK:
			((struct c_task_unit*)ptr)->next_free = (struct c_task_unit*)c->first_free;
			c->first_free = ((struct c_task_unit*)ptr);
			entries = CONT_TSK_PER_CONT;
			break;
		case CONT_ALLOC_SMO:
			((struct smo*)ptr)->next = (struct smo*)c->first_free;
			c->first_free = ((struct smo*)ptr);
			entries = CONT_SMO_PER_CONT;
			break;
		case CONT_ALLOC_MSG:
			((struct message*)ptr)->next = (struct message*)c->first_free;
			c->first_free = ((struct message*)ptr);
			entries = CONT_MSG_PER_CONT;
			break;
		case CONT_ALLOC_PRT:
			((struct port*)ptr)->next = (struct port*)c->first_free;
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
		c->prev = NULL;
		if(containers.first_free[type] != NULL)
			containers.first_free[type]->prev = c;
		containers.first_free[type] = c;
	}

	/* Will we return this page to the OS? */	
	if(((c->flags & CONT_FLAGS_PREALLOC) != CONT_FLAGS_PREALLOC) 
        && c->free == entries)
	{
		/*
		We will free the page, if total free structures
		of this type, are above a define
		*/
		if(containers.free[type] > containers.max_free[type])
		{
			/* Remove this container from the free list */
			if(c->prev == NULL)
				containers.first_free[type] = c->next;
			else
				c->prev->next = c->next;
			
			if(c->next != NULL)
				c->next->prev = c->prev;

            containers.free[type] -= entries;
			
			dyn_free_page(c, CONT_ALLOC2DYN(type));			
			return;
		}			
	}
}



