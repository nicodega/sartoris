/*  
 *   Sartoris microkernel arch-neutral task subsystem
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

/* multi-tasking implementation */

int create_task(int id, struct task *tsk, int *initm, unsigned int init_size) {

  char *dst2, *src2;
  int *dst;
  void *cached_mem_adr;
  unsigned int cached_size;
  int cached_priv_level;
  unsigned int i;
  int x;             /* mutex variable */
  int result, initm_result;

#ifdef COMPAT_REL53
  int j, new;	     /* temporary! */
#endif
  
  result = FAILURE;
  initm_result = FAILURE;  /* memory copy: failure */
  
  x = arch_cli();  /* enter critical block */
  
  if (VALIDATE_PTR(tsk)) {
  
    tsk = (struct task *) MAKE_KRN_PTR(tsk);
    cached_mem_adr = tsk->mem_adr;        /* if we will pagefault, we must do it now */
    cached_size = tsk->size;              /* (before the sanity checks) */
    cached_priv_level = tsk->priv_level;
    
    /* no page fault may occur after this point,
       the rest is truly atomic */

    if (0 <= id && id < MAX_TSK) {
      
      /* check if the memory initializing is allright */
      if (initm != NO_TASK_INIT) {    	// June 30 2005: I've changed this check to NO_TASK_INIT (defined as 0xFFFFFFFF)
					// because NULL is 0. And the task might be cloning itself.
	if (VALIDATE_PTR(initm)) {
	  if (init_size >= 0 && init_size <= cached_size) {
	    if (init_size != 0 && VALIDATE_PTR(((unsigned int)initm) + init_size - 1) && 
		!SUMOVERFLOW(((unsigned int)initm), init_size - 1)) {

	      initm_result = SUCCESS;

	    }
	  }
	}
      }
      
      /* check if the received task struct is ok */
      if (cached_priv_level >= 0) {
	if (tasks[id].state == DEFUNCT) {  /* only the truly dead shall be born again */

	  tasks[id].state = LOADING;
	  result = SUCCESS;

	}
      }
    }
  }    
  
  arch_sti(x); /* exit critical block */
  
  if (result == SUCCESS) {
  
    /* initialize everything else outisde critical block:
       create_task() has not returned yet so it's ok if we
       are interrupted, and state==LOADING, hence nobody will
       touch anything. */
  
    tasks[id].mem_adr = cached_mem_adr;
    tasks[id].size = cached_size;
    tasks[id].priv_level = cached_priv_level;
    tasks[id].thread_count = 0;
    /* tasks[address].map_count = 0; UNIMPLEMENTED => for mapSMOs */
    
    /* set every port as closed */
    
    for (i = 0; i < MAX_TSK_OPEN_PORTS; i++) {
      open_ports[id][i] = -1;
    }
    
    /* initialize memory if requested */
    
    if (initm != NO_TASK_INIT && initm_result == SUCCESS) {
      /* FIXME: re-check when arch_cpy_to_task is revised againt page faults */
      arch_cpy_to_task(id, (char*)initm, 0, init_size);
    }
  
  
#ifdef COMPAT_REL53
    /* magic: for sartoris 0.5.3 compatibility, open every port :( */
    for (j = 0; j < 32; j++) {
      new = create_port(&msgc, id);
      
      if (new != -1) {
	msgc.ports[new].mode = PRIV_LEVEL_ONLY;
	msgc.ports[new].priv = 3;
	
	open_ports[id][j] = new;
      }
    }
#endif
    
    if (arch_create_task(id, &tasks[id]) == 0) {
        /* the task is officially alive, other syscalls should
       operate on it, therefore: */
    
      tasks[id].state = ALIVE;
    } else {
      /* if arch_create fails, rollback everything */
      /* no need to delete_task_smos, there cannot be any */
      
      delete_task_ports(&msgc, id); /* <-- I think that only makes sense if    */
                                    /*      they were opened automatically for */
                                    /*      compatibility reasons              */

      tasks[id].state = DEFUNCT;    /* this must be done last */
      
      result = FAILURE;                        
    }
    
  }
  
  return result;
}

int destroy_task(int id) {
  
  struct task *tsk;
  int x;
  int result;
  
  result = FAILURE;
  
  x = arch_cli(); /* enter critical block */
  
  /* check everything */
  if ((0 <= id) && (id < MAX_TSK)) {
    
    tsk = &tasks[id];
    
    if (tsk->thread_count == 0 && 
	curr_task != id &&
	tsk->state == ALIVE) {
      result = SUCCESS;
      tsk->state = UNLOADING;
    }
    
  }
  
  arch_sti(x); /* exit critical block */
  
  if (result == SUCCESS) {
    
    delete_task_smo(&smoc, id);
    delete_task_ports(&msgc, id);
    
    tsk->size = 0;
    tsk->state = DEFUNCT;

    result = arch_destroy_task(id); /* if cannot destroy, set result accordingly */
    
  }
  
  return result;
}

int get_current_task(void) {
  return curr_task;
}
