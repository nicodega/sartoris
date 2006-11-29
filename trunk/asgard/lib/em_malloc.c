/*
*
*    	This malloc implementation is supposed to behave a little better than
*    	the previous implementation, by deciding where to start searching
*    	for free memory blocks using an array of pointers indexed by powers of 2
*
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

#define MALLOC_POINTER_MAXPOWER 16

static struct mem_desc *mem_free_first[MALLOC_POINTER_MAXPOWER];
#ifdef SAFE
static struct mutex malloc_mutex;	// for safe threading malloc
#endif

void init_mem(void *buffer, unsigned int size){

  struct mem_desc *mdes;
  int i = 0;

  // init buffer allocation method //

  mdes = (struct mem_desc*) buffer;
  mdes->size = size - sizeof(struct mem_desc);
  mdes->next = 0;
  mdes->prev = 0;

  // init free first array
  while(i < MALLOC_POINTER_MAXPOWER){mem_free_first[i] = (struct mem_desc*) buffer;}

  init_mutex(&malloc_mutex);
}

void close_malloc_mutex()
{
#ifdef SAFE
	close_mutex(&malloc_mutex);
#endif
}

void free(void *ptr);

void *malloc(m_size_t size)
{
	// perhaps m_malloc could be implemented in a different way than calloc //
	return calloc(size, 1);
}

void *calloc(m_size_t nelem, m_size_t elsize){

	int size, k, l, pow;
	struct mem_desc *i, *j;

	size = nelem * elsize;

	// see which pointer is to be used as free first
	k = 0;
	pow = 2;
	while(pow < size && k <= MALLOC_POINTER_MAXPOWER)
	{
		pow = (pow << 1);
		k++;
	}

	// find first fit //

// safe
#ifdef SAFE
	wait_mutex(&malloc_mutex);
#endif
// end safe

	if(mem_free_first[pow] != 0)
	{
		j = i = mem_free_first[pow];

		while(i != 0 && i->size < size)
		{
			j = i;
			i = i->next;
		}

		if(i == 0)
		{		
// safe
#ifdef SAFE
			leave_mutex(&malloc_mutex);
#endif
// end safe
			return 0;
		}

		if(i->size - size == 0 || i->size - size < (sizeof(struct mem_desc) + EM_TOLERANCE))
		{
			// eliminate this item from the list //
			if(i == mem_free_first[pow])
			{
				mem_free_first[pow] = i->next;
			}
			else
			{
				j->next = i->next;
			}
		}
		else
		{
			// create a new node on the list //
	   
			if(i == mem_free_first[pow])
			{
				j = mem_free_first[pow] = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size); 
			}
			else
			{
				j->next = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size);
				j = j->next;
			}
		
			j->size = i->size - size - sizeof(struct mem_desc);
			j->next = i->next; 
			j->prev = i;
			i->size = size;
		}

		// fix free first array
		k = 0;
		while(k < MALLOC_POINTER_MAXPOWER)
		{
			if(mem_free_first[k] == i)
			{
				// free first was pointing to the current free block
				// find a block on whitch this power of 2 fits (I perform a forward search
				// for on free this pointers are fixed too)
				if(k == 0) l = (int)i->next; // begin search on next free block
				
				if(l != 0)
				{
					// start searching forward
					while(((struct mem_desc *)l)->size < (2 << k) && l != 0)
					{
						l = (int)((struct mem_desc *)l)->next;
					}
				}
				
				mem_free_first[k] = (struct mem_desc *)l;
			}
			k++;
		}

	}
	else
	{
		  
		// safe
	#ifdef SAFE
		leave_mutex(&malloc_mutex);
	#endif
		// end safe
		return 0;
	}
	  
	  
		// safe
#ifdef SAFE
	leave_mutex(&malloc_mutex);
#endif
		// end safe
	return (void *) ((int) i + sizeof(struct mem_desc));
}

void free(void *ptr)
{
	int k;
	struct mem_desc *info, *i, *j, *available;

	info = (struct mem_desc*) ((int) ptr - sizeof(struct mem_desc));
  
  /* now let's get this piece of mem back on the list */

  
	// safe
#ifdef SAFE
	wait_mutex(&malloc_mutex);
#endif
	// end safe

	i = info->next;
	j = i->prev;

	// Coalesing //
	if( j != 0 && (j->size + (int)j + sizeof(struct mem_desc)) == (int) info)
	{
		if( i != 0 && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) )
		{
			// we can join j, info and i //
			j->size += info->size + i->size + (sizeof(struct mem_desc) << 1);
	
			// delete i from the list //
			j->next = i->next;
		}
		else
		{
			// we can put info an j together //
			j->size += info->size + sizeof(struct mem_desc);
		}
		available = j;
	}
	else if (i != 0 && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) )
	{
		if(j != 0) j->next = info;
	   
		info->next = i->next;
		info->size += i->size + sizeof(struct mem_desc);

		available = info;
	}
	else
	{
		// we cant't join anything //
		j->next = info;
		i->prev = info;
		info->prev = j;
		info->next = i;

		available = info;
	}

	// fix free first array
	// NOTE: here we check for each power of 2 if new available size
	// is greater than required, if so, the pointer is updated.
	k = 0;
	while(k < MALLOC_POINTER_MAXPOWER)
	{
		if( (mem_free_first[k] == 0 || (unsigned int)mem_free_first[k] > (unsigned int)available) && info->size >= (2 << k))
		{
			mem_free_first[k] = available;
		}
		k++;
	}
	

// safe
#ifdef SAFE
	leave_mutex(&malloc_mutex);
#endif
// end safe
}

