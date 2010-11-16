
#include <lib/malloc.h>

struct mem_desc *mem_free_first;

void init_mem(void *buffer, unsigned int size){

  struct mem_desc *mdes;

  // init buffer allocation method //

  mdes = (struct mem_desc *) buffer;
  mdes->size = size - sizeof(struct mem_desc);
  mdes->next = 0;
  mem_free_first = (struct mem_desc *) buffer;
}

void *malloc(unsigned int data_size, unsigned int length){

  int size;
  struct mem_desc *i, *j;

  size = data_size * length;

  // find the first fit //

  if(mem_free_first != 0){
    j = i = mem_free_first;

    while(i != 0 && i->size < size){
      j = i;
      i = i->next;
    }

    if(i == 0){
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
	j = mem_free_first = (struct mem_desc *) ((int)i + sizeof(struct mem_desc) + size); 
      }else{
	j->next = (struct mem_desc *) ((int)i + sizeof(struct mem_desc) + size);
	j = j->next;
      }
      j->size = i->size - size - sizeof(struct mem_desc);
      j->next = i->next; 
      i->size = size;
    }

  }else{
    return 0;
  }
  
  return (void *) ((int) i + sizeof(struct mem_desc));
}

void free(int *p){

  struct mem_desc *info, *i, *j;

  /* this is a little more complex than the other function.
     In fact it's really slower than malloc, but this
     descision has been taken, because this should be called
     less often. */

  info = (struct mem_desc *) ((int) p - sizeof(struct mem_desc));
  
  /* now let's get this piece of mem back on the list */

  if(mem_free_first != 0){

    j = i = mem_free_first;

    while(i != 0 && ((int)i < (int)info)){
      j = i;
      i = i->next;
    }

    // Coalesing //

    if( (j->size + (int)j + sizeof(struct mem_desc)) == (int) info){
      if( i != 0 && ((info->size + (int)info + sizeof(struct mem_desc)) == (int) i) ){
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

}
