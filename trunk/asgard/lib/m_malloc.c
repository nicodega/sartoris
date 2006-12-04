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


#include <lib/malloc.h>
#include <lib/const.h>


#define TOLERANCE 0 //sizeof(struct mem_desc) + 20

struct mem_desc{
  int size;
  struct mem_desc *next;
};


static unsigned int upbound;
static unsigned int downbound;
static struct mem_desc *mem_free_first;
#ifdef SAFE
static struct mutex malloc_mutex;	// for safe threading malloc
#endif
static unsigned int m_free_mem;

// TODO: There seemes to be a problem when malloc is called with size 0. FIXIT.

void init_mem(void *buffer, unsigned int size)
{
	struct mem_desc *mdes;

	// init buffer allocation method //
	mdes = (struct mem_desc*) buffer;
	mdes->size = size - sizeof(struct mem_desc);
	mdes->next = 0;
	mem_free_first = (struct mem_desc*) buffer;

	upbound = (unsigned int)buffer + size;
	downbound = (unsigned int)buffer;
	m_free_mem = size;

#	ifdef SAFE
	init_mutex(&malloc_mutex);
#	endif
}

void close_malloc_mutex()
{
#	ifdef SAFE
	close_mutex(&malloc_mutex);
#	endif
}

void *ecalloc(size_t nelem, size_t elsize, int zero);

void *malloc(size_t size)
{
	// perhaps m_malloc could be implemented in a different way than calloc //
	return ecalloc(size, 1, 0);
}

void *calloc(size_t nelem, size_t elsize)
{
	return ecalloc(nelem, elsize, 1);
}

void *ecalloc(size_t nelem, size_t elsize, int zero)
{
	unsigned int size,k;
	struct mem_desc*i, *j;
	unsigned char *ptr = NULL;

	size = nelem * elsize;

	// find first fit //
#	ifdef SAFE
	wait_mutex(&malloc_mutex);
#	endif
	
	if(mem_free_first != 0)
	{
		j = i = mem_free_first;

		while(i != 0 && i->size < size)
		{
			j = i;
		i = i->next;
		}

		if(i == 0)
		{
#			ifdef SAFE
			leave_mutex(&malloc_mutex);
#			endif
			return NULL;
		}

		if(i->size - size == 0 || i->size - size < (sizeof(struct mem_desc) + TOLERANCE))
		{
			// eliminate this item from the list //
			if(i == mem_free_first)
			{
				mem_free_first = i->next;
			}
			else
			{
				j->
				next = i->next;
			}

		}
		else
		{
			// create a new node on the list //
			if(i == mem_free_first)
			{
				mem_free_first = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size); 
				mem_free_first->size = i->size - size - sizeof(struct mem_desc);
				mem_free_first->next = i->next; 
			}
			else
			{
				j->next = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size);
				j->next->size = i->size - size - sizeof(struct mem_desc);
				j->next->next = i->next; 
			}
			i->size = size;
		}

	}
	else
	{
#		ifdef SAFE
		leave_mutex(&malloc_mutex);
#		endif
		return NULL;
	}

#	ifdef SAFE
	leave_mutex(&malloc_mutex);
#	endif
	
	m_free_mem -= size;

	ptr = (unsigned char*)((int) i + sizeof(struct mem_desc));
		
	/* Zero out memory */
	if(zero)
	{
		for(k=0; k < size;k++){ptr[k]=0;}
	}
	return (void *)ptr;
}

void free(void *ptr)
{

	struct mem_desc*info, *i, *j;
	unsigned int size;

	if((unsigned int)ptr < downbound || (unsigned int)ptr > upbound) 
		return;

	info = (struct mem_desc*) ((int) ptr - sizeof(struct mem_desc));

	/* now let's get this piece of mem back on the list */
	size = info->size;

#	ifdef SAFE
	wait_mutex(&malloc_mutex);
#	endif

	if(mem_free_first != 0)
	{
		j = i = mem_free_first;

		while(i != 0 && ((int)i < (int)info))
		{
			j = i;
			i = i->next;
		}

		// Coalesing //
		if( (j->size + (int)j + sizeof(struct mem_desc)) == (int) info)
		{
			if( i != 0 && i != j && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) )
			{
				// we can join j, info and i //
				j->size += info->size + i->size + sizeof(struct mem_desc)*2;
				// delete i from the list //
				j->next = i->next;
			}
			else
			{
				// we can put info an j together //
				j->size += info->size + sizeof(struct mem_desc);
			}
		}
		else if (i != 0 && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) )
		{
			if(mem_free_first == i)
			{
				mem_free_first = info;
			}
			else
			{
				j->next = info;
			}
			info->next = i->next;
			info->size += i->size + sizeof(struct mem_desc);
		}
		else
		{
			// we cant't join anything //
			j->next = info;
			info->next = i;
		}
	}
	else
	{
		mem_free_first = info;
		info->next = 0;
	}

#	ifdef SAFE
	leave_mutex(&malloc_mutex);
#	endif
	
	m_free_mem += size;
}

// returns free memory
unsigned int free_mem()
{	
    return m_free_mem;
}

