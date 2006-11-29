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

static unsigned int upbound;
static unsigned int downbound;
static struct mem_desc *mem_free_first;
#ifdef SAFE
static struct mutex malloc_mutex;	// for safe threading malloc
#endif
static unsigned int m_free_mem;

// TODO: There seemes to be a problem when malloc is called with size 0. FIXIT.

void init_mem(void *buffer, unsigned int size){

  struct mem_desc *mdes;

  // init buffer allocation method //

  mdes = (struct mem_desc*) buffer;
  mdes->size = size - sizeof(struct mem_desc);
  mdes->next = 0;
  mem_free_first = (struct mem_desc*) buffer;

  upbound = (unsigned int)buffer + size;
  downbound = (unsigned int)buffer;
  m_free_mem = size;
#ifdef SAFE
  init_mutex(&malloc_mutex);
#endif
}

void close_malloc_mutex()
{
#ifdef SAFE
	close_mutex(&malloc_mutex);
#endif
}

void free(void *ptr);

void *malloc(size_t size)
{
	// perhaps m_malloc could be implemented in a different way than calloc //
	return calloc(size, 1);
}

void *calloc(size_t nelem, size_t elsize){

  unsigned int size;
  struct mem_desc*i, *j;

  size = nelem * elsize;

  // find first fit //

  // safe
#ifdef SAFE
  wait_mutex(&malloc_mutex);
#endif
  // end safe

  if(mem_free_first != 0){
    j = i = mem_free_first;

    while(i != 0 && i->size < size){
      j = i;
      i = i->next;
    }

    if(i == 0){
			
	// safe
#ifdef SAFE
	leave_mutex(&malloc_mutex);
#endif
	// end safe
      return 0;
    }

    if(i->size - size == 0 || i->size - size < (sizeof(struct mem_desc) + TOLERANCE)){

      // eliminate this item from the list //

      if(i == mem_free_first){
	mem_free_first = i->next;
      }else{
	j->next = i->next;
      }

    }else{
      // create a new node on the list //
   
      if(i == mem_free_first){
	mem_free_first = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size); 
	mem_free_first->size = i->size - size - sizeof(struct mem_desc);
	mem_free_first->next = i->next; 
      }else{
	j->next = (struct mem_desc*) ((int)i + sizeof(struct mem_desc) + size);
	j->next->size = i->size - size - sizeof(struct mem_desc);
      	j->next->next = i->next; 
      }
      i->size = size;
    }

  }else{
	  
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
	m_free_mem -= size;

  return (void *) ((int) i + sizeof(struct mem_desc));
}

void free(void *ptr){

  struct mem_desc*info, *i, *j;
  unsigned int size;

  if((unsigned int)ptr < downbound || (unsigned int)ptr > upbound) 
	 return;// for(;;);

  info = (struct mem_desc*) ((int) ptr - sizeof(struct mem_desc));
  
  /* now let's get this piece of mem back on the list */
size = info->size;
  
	// safe
#ifdef SAFE
	wait_mutex(&malloc_mutex);
#endif
	
	// end safe

  if(mem_free_first != 0){

    j = i = mem_free_first;

    while(i != 0 && ((int)i < (int)info)){
      j = i;
      i = i->next;
    }

    // Coalesing //

    if( (j->size + (int)j + sizeof(struct mem_desc)) == (int) info){
      if( i != 0 && i != j && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) ){
	// we can join j, info and i //
	j->size += info->size + i->size + sizeof(struct mem_desc)*2;
	// delete i from the list //
	j->next = i->next;
      }else{
	// we can put info an j together //
	j->size += info->size + sizeof(struct mem_desc);
      }
    }else if (i != 0 && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) ){
      if(mem_free_first == i){
	mem_free_first = info;
      }else{
	j->next = info;
      }
      info->next = i->next;
      info->size += i->size + sizeof(struct mem_desc);
    }else{
      // we cant't join anything //
      j->next = info;
      info->next = i;
    }

  }else{
    mem_free_first = info;
    info->next = 0;
  }

	// safe
#ifdef SAFE
	leave_mutex(&malloc_mutex);
#endif
	// end safe
m_free_mem += size;
}

// returns free memory
unsigned int free_mem()
{	
    return m_free_mem;
}

