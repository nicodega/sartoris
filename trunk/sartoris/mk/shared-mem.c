
/*  
 *   Sartoris shared memory object management 
 *   system calls & algorithms implementation
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"

#include "sartoris/kernel-data.h"



/* shared memory subsystem implementation */

/* these are the proper system calls: */

int share_mem(int target_task, int addr, int size, int rw) {

    int x, result;
    
    result = FAILURE;
    
    if (0 <= target_task && target_task < MAX_TSK) {
      
      x = arch_cli(); /* enter critical block */
      
      if (tasks[target_task].state == ALIVE) {
	
	if (VALIDATE_PTR(addr) && VALIDATE_PTR(addr + size - 1)) {
    
	  result =  get_new_smo(&smoc, curr_task, target_task, addr, size, rw);
	}
      }
      
      arch_sti(x); /* exit critical block */
    }
    
    return result;
}

int claim_mem(int smo_id) {
    int result;
 
    result = FAILURE;
    
    /* no need to block here, smo_id is in my stack */
    if (0 <= smo_id && smo_id < MAX_SMO) {
      /* the rest of the validation is in delete_smo */
      result = delete_smo(&smoc, curr_task, smo_id);
    }
    
    return result;
}

int read_mem(int smo_id, int off, int size, int *dest) {
    struct smo *my_smo;
    int src_task;
    char *src;
    int x, result;
    
    result = FAILURE;

    if (0 <= smo_id && smo_id < MAX_SMO &&
	off >= 0 && size >= 0) {
      
      x = arch_cli(); /* enter critical block */

      if (VALIDATE_PTR(dest)) {

	my_smo = &(smoc.smos[smo_id]);


	while (my_smo->valid && my_smo->target == curr_task &&
	       off + size <= my_smo->len && (my_smo->rights & READ_PERM) &&  /* FIXME! that should be < my_smo->len, */
	       result != SUCCESS) {                                          /*  but OS relies on <= */

	  int bytes;

	  src = (char *) ((unsigned int)my_smo->base + off);

	  bytes = arch_cpy_from_task(my_smo->owner, (char*)src, (char*)dest, size);  /* FIXME after cpy from task is fixed */

	  off += bytes;
	  size -= bytes;

	  if (size == 0) {
	    result = SUCCESS; 
	  }
	  
	}
	
      }
      
      arch_sti(x); /* exit critical block */
      
    }
    
    return result;
}

int write_mem(int smo_id, int off, int size, int *src) {
    struct smo *my_smo;
    int src_task;
    char *dest;
    int x, result;
    
    result = FAILURE;
    
    if (0 <= smo_id && smo_id < MAX_SMO &&
	off >= 0 && size >= 0) {
    
        x = arch_cli(); /* enter critical block */
    
	if (VALIDATE_PTR(src)) {

	  my_smo = &(smoc.smos[smo_id]);

	  while (my_smo->valid && my_smo->target == curr_task &&
		 off + size <= my_smo->len && (my_smo->rights & WRITE_PERM) &&  /* FIXME! that should be < my_smo->len, */
		 result != SUCCESS) {                                           /*  but OS relies on <= */
	    
	    int bytes;
	    
	    dest = (char *) ((unsigned int)my_smo->base + off);
	    
	    bytes = arch_cpy_to_task(my_smo->owner, (char*)src, (char*)dest, size); /* FIXME after cpy to task is fixed */
	    
	    off += bytes;
	    size -= bytes;
	    
	    if (size == 0) {
	      result = SUCCESS;
	    }

	  }
	}
	
	arch_sti(x); /* exit critical block */
    }
    
    return result;
}

int pass_mem(int smo_id, int target_task) {
    struct smo *smo;
    int x, result;
    
    result = FAILURE;
    
    if (0 <= smo_id && smo_id < MAX_SMO &&
	0 <= target_task && target_task < MAX_TSK) {
   
       x = arch_cli(); /* enter critical block */
       
       smo = &(smoc.smos[smo_id]);
       
       if (smo->valid && smo->target == curr_task) {
	 
	  smo->target = target_task;
	  result = SUCCESS;
       }
	 
       arch_sti(x); /* exit critical block */
    
    }
    
    return result;
}

int mem_size(int smo_id) {
    
  struct smo *smo;
  int result;
  
  result = -1;

  if (0 <= smo_id && smo_id < MAX_SMO) {
    
    smo = &(smoc.smos[smo_id]);
       
    if (smo->valid && smo->target == curr_task) {
      
      result = smo->len;
    
    }
  }
  
  return result;
  
}




/* the following functions implement the data structures used above: */

void init_smos(struct smo_container *smoc) {
    int i;

    for (i = 0; i < MAX_TSK; i++) {
	smoc->task_first_smo[i] = -1;
	smoc->task_num_smo[i] = 0;
    }

    smoc->smos[MAX_SMO - 1].next = -1;
    smoc->smos[MAX_SMO - 1].valid = 0;

    for (i = 0; i < MAX_SMO - 1; i++) {
	smoc->smos[i].next = i + 1;
	smoc->smos[i].valid = 0;
    }

    smoc->free_first = 0;
}

/* this function is called within a critical block */
int get_new_smo(struct smo_container *smoc, int task, int target_task,
		int addr, int size, int perms) {
    int new, next;

    new = smoc->free_first; /* first SMO of free list */

    if (new >= 0) { /* list could have been empty... */
      
        /* second element (possibly nil) of free list is now first */
	smoc->free_first = smoc->smos[new].next;
	
	/* update task's doubly linked list of SMOs */
	smoc->smos[new].next = next = smoc->task_first_smo[task];
	smoc->smos[new].prev = -1;
	
	if (next != -1) {
	    smoc->smos[next].prev = new;
	}

	smoc->task_first_smo[task] = new;
	smoc->task_num_smo[task]++;
	
	/* copy new SMO parameters */
	smoc->smos[new].valid = 1;
	smoc->smos[new].owner = task;
	smoc->smos[new].target = target_task;
	smoc->smos[new].base = addr;
	smoc->smos[new].len = size;
	smoc->smos[new].rights = perms;
	
    }
    
    return new;
}

int delete_smo(struct smo_container *smoc, int task, int id) {
    int x, result;
    int prev, next;
    
    result = FAILURE;
    
    x = arch_cli(); /* enter critical block */

    if (smoc->smos[id].owner == task && smoc->smos[id].valid) {
	
        result = SUCCESS;
        
        prev = smoc->smos[id].prev;
	next = smoc->smos[id].next;
	
	if (smoc->task_first_smo[task] == id) {
	    smoc->task_first_smo[task] = smoc->smos[id].next;
	}

	if (prev != -1) {
	    smoc->smos[prev].next = next;
	}
	if (next != -1) {
	    smoc->smos[next].prev = prev;
	}

	smoc->smos[id].valid = 0;
	smoc->smos[id].next = smoc->free_first;
	smoc->free_first = id;

	smoc->task_num_smo[task]--;

    }

    arch_sti(x); /* exit critical block */
    
    return result;
}

void delete_task_smo(struct smo_container *smoc, int task_id) {
    int i, j, x;
    int old_free_first;

    x = arch_cli(); /* enter critical block */

    i = smoc->task_first_smo[task_id];
    smoc->task_first_smo[task_id] = -1;
    smoc->task_num_smo[task_id] = 0;

    if (i != -1) {
	old_free_first = smoc->free_first;
	smoc->free_first = i;

	while (i != -1) {	/* this loop is entered at least once */
	    j = i;
	    smoc->smos[j].valid = 0;
	    i = smoc->smos[j].next;
	}

	smoc->smos[j].next = old_free_first;
    }

    arch_sti(x); /* exit critical block */
}






















