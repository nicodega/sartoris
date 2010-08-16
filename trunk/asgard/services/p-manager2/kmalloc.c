/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

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
#include "kmalloc.h"
#include "types.h"
#include "vm.h"

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
		pman_mem.free_directories[i] = 0;
	}
	
	pman_mem.total_pages = 0;
	pman_mem.total_mem = 0;
	pman_mem.fixed_pages_start = start_mem;
	pman_mem.fixed_pages = start_pages_count;
}

/* Get the index of a descriptor with free Buckets from a given directory */
UINT32 get_free(struct kmem_directory_page *des_dir)
{
	struct kmem_page_descriptor *d = NULL;

	if(des_dir == NULL) return 0xFFFFFFFF;

	/* First go thorugh allocated descriptors */
	d = des_dir->first_descriptor;
	while(d != NULL)
	{
		if(d->free_buckets != 0) return d->index;
		d = d->next_descriptor;
	}

	d = des_dir->first_na_descriptor;
	while(d != NULL)
	{
		if(d->free_buckets != 0) return d->index;
		d = d->next_descriptor;
	}

	return 0xFFFFFFFF;
}

/* Gets a Directory page */
struct kmem_directory_page *create_directory(UINT16 bucket_size)
{
	struct kmem_directory_page *p = NULL;
	struct kmem_page_descriptor *d = NULL;
	int i, index;

	p = (struct kmem_directory_page *)vmm_pm_get_page(FALSE);

	if(p == NULL)
	{
		return NULL;
	}

	/* Initialize directory Page Descriptors */
	p->first_na_descriptor = p->d;
	p->first_descriptor = NULL;
	p->allocated_descriptors = 0;
	p->free_alloc_descriptors = 0;
	p->magic = 0xDEADDEAD;
	
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
		p->d[i].index = i;

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

	if(pman_mem.directory[index] != NULL)
		pman_mem.directory[index]->prev = p;
	
	pman_mem.directory[index] = p;

	pman_mem.total_pages++;
	pman_mem.total_directories[index]++;

	return p;
}


/* Create a new Buckets Page for a descriptor */
struct kmem_page *create_buckets_page(UINT16 bucket_size)
{
	struct kmem_free_node *fn = NULL;
	UINT32 i, buckets;
	BYTE *p;
	
	buckets = (0x1000 / bucket_size);

	/* Get a page from vm */
	p = (BYTE*)vmm_pm_get_page(FALSE);

	if(p == NULL) 
	{
		return NULL;
	}

	/* Initialize free nodes on page. */
	for(i = 0; i < buckets - 1; i++)
	{
		fn = (struct kmem_free_node*)((UINT32)p + i * bucket_size);

		fn->next_free = (struct kmem_free_node*)((UINT32)fn + bucket_size);		
		fn->magic = 0xDEADDEAD;		
	}

	/* End the list with a NULL value */
	fn = ((struct kmem_free_node*)((UINT32)p + (buckets-1) * bucket_size));
	fn->next_free = NULL;
	fn->magic = 0xDEADDEAD;

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
		if(des_dir == NULL)
		{

			return NULL;
		}
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
			if(des_dir->first_na_descriptor != NULL)
				des_dir->first_na_descriptor->prev_descriptor = des_dir->d[i].prev_descriptor;
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
	
	if(des_dir->d[i].free_first->magic != 0xDEADDEAD)
		pman_print_and_stop("KMALLOC: Allocating on a faulty buffer (magic number is not correct) ");

	/* Set up header */
	h = (struct kmem_header *)des_dir->d[i].free_first;
	h->descriptor = &des_dir->d[i];
	h->magic_preserve = 0;

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
	struct kmem_free_node *n,*fnode;
	struct kmem_header *h;

	if(ptr == NULL) return;

	h = (struct kmem_header*)((UINT32)ptr - sizeof(struct kmem_header));
	fnode = (struct kmem_free_node *)h;

	if(fnode->magic == 0xDEADDEAD || fnode->magic != 0)
		pman_print_and_stop("ATTEMPT TO FREE BAD PTR");

	/* Get descriptor from allocation header. */
	desc = h->descriptor;

	/* Fix Free list of buckets */
	desc->free_buckets++;
	n = desc->free_first;
	
	pman_mem.total_mem -= desc->bucket_size;
	desc->free_first = fnode;
	desc->free_first->next_free = n;
	desc->free_first->magic = 0xDEADDEAD;

	if(desc->free_buckets == (0x1000 / desc->bucket_size))
	{		
		/* Free this descriptor */
		INT32 index = BUCKET_SIZE_INDEX(desc->bucket_size);
					
		/* Remove descriptor from free list */
		if(desc->directory->first_descriptor == desc)
		{
			desc->directory->first_descriptor = desc->next_descriptor;
			if(desc->next_descriptor != NULL) desc->directory->first_descriptor->prev_descriptor = NULL;
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
		desc->prev_descriptor = NULL;
		desc->directory->first_na_descriptor = desc;

		desc->directory->allocated_descriptors--;
		pman_mem.total_pages--;

		/* Free buckets page. */
		vmm_pm_put_page(desc->page);
		desc->page = NULL;
		desc->free_first = NULL;
		desc->flags = (desc->flags & (~KMEM_DFLAG_ALLOCATED));
		
		/* Check if there are no more descriptors on this directory */		
		if(desc->directory->allocated_descriptors == 0)
		{
			/* Remove this directory */
			pman_mem.total_directories[index]--;

			// fix directory pages list
			if(desc->directory == pman_mem.directory[index])
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
