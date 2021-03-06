/*
	Process Manager Malloc routine.

	This routine will allow memory allocation for small (<= 4k - 4 bytes) and big (>= 4k) memory blocks.

	- Small memory blocks will be padded to a power of 2 for allocation.
	- Large memory blocks will be taken out from a pman heap (and padded to a power of 2)
	- I believe for small memory blocks, on the worst case scenario, many pages could be allocated
	  but this could be very big only if there where bursts of allocation with odd patterns.

	NOTE: If kalloc is used for structures smaller than 16 bytes internal fragmentation will be huge,
		  for the lower power of 2 considered is 32. (i.e all sizes smaller than 32 will be padded to 32 bytes)
*/
#include "include/kmalloc.h"
#include "include/types.h"
#include "include/vm.h"

struct kmem pman_mem;

void kmem_init(UINT32 start_mem, UINT32 start_pages_count)
{
	UINT32 i;

	/* 
		Initialize directories: Initially no directories will be allocated
	*/
	for(i = 0; i < 7; i++)
	{
		pman_mem.directory[i] = NULL;
		pman_mem.total_directories[i] = 0;
	}
	
	pman_mem.total_pages = 0;
	pman_mem.total_mem = 0;
	pman_mem.fixed_pages_start = start_mem;
	pman_mem.fixed_pages = start_pages_count;
}

/* Get the index of a descriptor with free Buckets from a given directory */
UINT32 get_free(struct kmem_directory_page *des_dir)
{
	UINT32 i;

	if(des_dir == NULL) return 0xFFFFFFFF;

	for(i = 0; i < PAGE_DESCRIPTORS; i++)
	{
		if(des_dir->d[i].free_buckets != 0) return i;
	}

	return 0xFFFFFFFF;
}

/* Alloca a Directory page */
struct kmem_directory_page *create_directory(UINT16 bucket_size)
{
	struct kmem_directory_page *p = (struct kmem_directory_page *)vmm_pm_get_page(FALSE);
	struct kmem_page_descriptor *d = NULL;
	int i, index;

	if(p == NULL) return NULL;

	/* Initialize directory Page Descriptors */
	p->first_na_descriptor = p->d;
	p->first_descriptor = NULL;
	p->allocated_descriptors = 0;
	
	for(i = 0; i < PAGE_DESCRIPTORS; i++)
	{
		p->d[i].bucket_size = bucket_size;

		/* Not allocated */
		p->d[i].flags = KMEM_DFLAG_NONE;
		p->d[i].page = NULL;

		/* Set free descriptors List */
		p->d[i].free_buckets = 0x1000 / bucket_size;
		p->d[i].free_first = NULL;

		p->d[i].directory = p;

		/* Descriptors list (Initially all of them are non-allocated) */
		p->d[i].prev_descriptor = d;
		d = &p->d[i];

		if(i < PAGE_DESCRIPTORS-1)
			p->d[i].next_descriptor = &p->d[i+1];
		else
			p->d[i].next_descriptor = NULL;
	}
	
	index = BUCKET_SIZE_INDEX(bucket_size);

	/* Add the directory to the directories linked list */
	p->next = pman_mem.directory[index];
	p->prev = NULL;

	if(pman_mem.directory[index] == NULL)
	{
		pman_mem.directory[index] = p;
	}
	else
	{
		pman_mem.directory[index]->prev = p;
		pman_mem.directory[index] = p;
	}

	pman_mem.total_pages++;
	pman_mem.total_directories[index]++;

	return p;
}


/* Create a new Buckets Page for a descriptor */
struct kmem_page *create_buckets_page(UINT16 bucket_size)
{
	struct kmem_free_node *fn = NULL;
	UINT32 i, buckets = (0x1000 / bucket_size);

	/* Get a page from vm */
	BYTE *p = (BYTE*)vmm_pm_get_page(FALSE);

	if(p == NULL) return NULL;

	/* Initialize free nodes on page. */
	for(i = 0; i < buckets - 1; i++)
	{
		fn = (struct kmem_free_node*)((UINT32)p + i * bucket_size);
		fn->next_free = (struct kmem_free_node*)((UINT32)fn + bucket_size);
	}

	/* End the list with a NULL value */
	((struct kmem_free_node*)((UINT32)p + (buckets-1) * bucket_size))->next_free = NULL;

	pman_mem.total_pages++;

	return (struct kmem_page *)p;
}

/* Allocation. */
ADDR kmalloc(UINT32 size)
{
	struct kmem_free_node *fnode;
	struct kmem_header *h;
	UINT32 bucket_size, i;
	struct kmem_directory_page *des_dir;

	if(size == 0 || size > 2048) return NULL;

	/* Which bucket size should we use? */
	bucket_size = BUCKET_SIZE(size);

	/* Get the first directory for this bucket size */
	des_dir = pman_mem.directory[BUCKET_SIZE_INDEX(bucket_size)];

	/* Find a directory with free buckets and an index to the free bucket */
	i = get_free(des_dir);

	while(des_dir != NULL && i == 0xFFFFFFFF)
	{
		des_dir = des_dir->next;
		i = get_free(des_dir);
	}

	/* If we got to the last directory, we have to create a new directory */
	if(des_dir == NULL)
	{
		/* create a new descriptor directory */
		des_dir = create_directory(bucket_size);
		i = 0;

		if(des_dir == NULL) return NULL;
	}

	/* Check if descriptor is allocated */
	if(!(des_dir->d[i].flags & KMEM_DFLAG_ALLOCATED))
	{
		/* Allocate a new buckets page for the free descriptor. */
		des_dir->d[i].page = create_buckets_page(bucket_size);

		/* Set descriptor as allocated */
		des_dir->d[i].flags |= KMEM_DFLAG_ALLOCATED;

		des_dir->d[i].free_first = (struct kmem_free_node *)des_dir->d[i].page;
		des_dir->d[i].free_buckets = 0x1000 / bucket_size;

		/* Remove descriptor from na linked list */
		if(des_dir->first_na_descriptor == &des_dir->d[i])
		{
			des_dir->first_na_descriptor = des_dir->d[i].next_descriptor;
			if(des_dir->d[i].next_descriptor != NULL)
				des_dir->d[i].next_descriptor->prev_descriptor = des_dir->d[i].prev_descriptor;
		}
		else
		{
			if(des_dir->d[i].prev_descriptor != NULL)
				des_dir->d[i].prev_descriptor->next_descriptor = des_dir->d[i].next_descriptor;

			if(des_dir->d[i].next_descriptor != NULL)
				des_dir->d[i].next_descriptor->prev_descriptor = des_dir->d[i].prev_descriptor;
		}

		/* Add descriptor to the allocated linked list */
		des_dir->d[i].next_descriptor = des_dir->first_descriptor;
		des_dir->first_descriptor = &des_dir->d[i];
		des_dir->d[i].prev_descriptor = NULL;
		if(des_dir->d[i].next_descriptor != NULL)
			des_dir->d[i].next_descriptor->prev_descriptor = &des_dir->d[i];

		
		des_dir->allocated_descriptors++;
	}
    
	/* Allocate the bucket */
	fnode = des_dir->d[i].free_first->next_free;

	/* Set up header */
	h = (struct kmem_header *)des_dir->d[i].free_first;

	h->descriptor = &des_dir->d[i];

	/* Fix free list */
	des_dir->d[i].free_first = fnode;
	des_dir->d[i].free_buckets--;

	pman_mem.total_mem += bucket_size;

	return (ADDR)((UINT32)h + sizeof(struct kmem_header));
}

/* De-Allocation. */
void kfree(ADDR ptr)
{
	struct kmem_page_descriptor *desc;
	struct kmem_free_node *n;

	if(ptr == NULL) return;

	/* Get descriptor from allocation header. */
	desc = ((struct kmem_header*)((UINT32)ptr - sizeof(struct kmem_header)))->descriptor;

	/* Fix Free list of buckets */
	desc->free_buckets++;
	n = desc->free_first;

	desc->free_first = (struct kmem_free_node *)((UINT32)ptr - sizeof(struct kmem_header));

	desc->free_first->next_free = n;

	pman_mem.total_mem -= desc->bucket_size;

	/* If descriptor has all its buckets free, increment free_pages. */
	if(desc->free_buckets == (0x1000 / desc->bucket_size))
	{
		/* Free this descriptor */
		INT32 index = BUCKET_SIZE_INDEX(desc->bucket_size);
			
		/* Remove descriptor from free list */
		if(desc->directory->first_descriptor == desc)
		{
			desc->directory->first_descriptor = desc->next_descriptor;
		}
		else
		{
			if(desc->prev_descriptor != NULL)
				desc->prev_descriptor->next_descriptor = desc->next_descriptor;

			if(desc->next_descriptor != NULL)
				desc->next_descriptor->prev_descriptor = desc->prev_descriptor;
		}

		/* Add descriptor to na list */
		if(desc->directory->first_na_descriptor != NULL)
			desc->directory->first_na_descriptor->prev_descriptor = desc;
		desc->next_descriptor = desc->directory->first_na_descriptor;
		desc->directory->first_na_descriptor = desc;

		desc->directory->allocated_descriptors--;
		pman_mem.total_pages--;

		/* Free buckets page. */
		vmm_pm_put_page(desc->page);
		desc->page = NULL;
		desc->flags &= ~KMEM_DFLAG_ALLOCATED;
			
		/* Check if there are no more descriptors on this directory */		
		if(desc->directory->allocated_descriptors == 0)
		{
			/* Remove this directory */
			pman_mem.total_directories[index]--;

			// fix directory pages list
			if(desc->directory->prev == NULL)
				pman_mem.directory[index] = desc->directory->next;
			else
				desc->directory->prev->next = desc->directory->next;

			if(desc->directory->next != NULL)
				desc->directory->next->prev = desc->directory->prev;

			// free the page
			vmm_pm_put_page(desc->directory);
				
			desc->directory = NULL;

			pman_mem.total_pages--;
		}		
	}
}

unsigned char *pages[0x1000 * 20];
BOOL fpages[20];

int main(int argc, char **args)
{
	int i =0, j;
	int *ip[200];
	char *c;

	for(i = 0; i < 20; i++)
	{
		fpages[i] = TRUE;
		for(j = 0; j < 0x1000; j++){pages[(i << 12) + j] = 0;}
	}

	kmem_init(0, 0);

	for(i = 0; i < 200; i++)
	{
		ip[i] = (int*)kmalloc(sizeof(int));	
		*ip[i] = i;
	}
    
    c = (char*)kmalloc(40);

	for(i = 0; i < 200; i++)
	{
		if(*ip[i] != i)
		{
			j = 0;
		}
	}	

	kfree(c);

	for(i = 0; i < 200; i++)
	{
		kfree(ip[i]);
	}	

	for(i = 0; i < 20; i++)
	{
		if(fpages[i] = FALSE)
		{
			j = 0;
		}		
	}
}

void vmm_pm_put_page(ADDR addr)
{
	int i = (((UINT32)addr - (UINT32)pages) >> 12), j;
	fpages[i] = TRUE;
	for(j = 0; j < 0x1000; j++){pages[(i << 12) + j] = 0;}
}

ADDR vmm_pm_get_page(BOOL lowmem)
{
	int i = 0;
	UINT32 ptr;
	for(i = 0; i < 20; i++)
	{
		if(fpages[i] == TRUE)
		{
			fpages[i] = FALSE;
			ptr = (i << 12);
			return (ADDR)((UINT32)pages + ptr);
		}
	}
	return NULL;
}

