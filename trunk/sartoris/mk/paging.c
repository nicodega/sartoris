/*  
 *   Sartoris microkernel arch-neutral paging functions
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

/* paging implementation */

int page_in(int task, void *linear, void *physical, int level, int attrib) {
#ifdef PAGING
    return arch_page_in(task, linear, physical, level, attrib);
#else
    return FAILURE;
#endif

}

int page_out(int task, void *linear, int level) {
#ifdef PAGING
    return arch_page_out(task, linear, level);
#else
    return FAILURE;
#endif
}

int flush_tlb() {
#ifdef PAGING
    return arch_flush_tlb();
#else
    return FAILURE;
#endif
}

int get_page_fault(struct page_fault *pf) {
  
  int result;
  int x;

  result = FAILURE;

#ifdef PAGING
  
  if (last_page_fault.task_id != -1) {
    /* no need for atomicity here, if != -1 then always != -1
       (yeah, it sounds a lot like temporal logic) */
    
    x = arch_cli(); /* enter critical block */
    
    pf = MAKE_KRN_PTR(pf);
    
    if (VALIDATE_PTR(pf) && VALIDATE_PTR(pf + sizeof(struct page_fault) - 1)) {
      
      *pf = last_page_fault;
      result = SUCCESS;

    }
    
    arch_sti(x); /* exit critical block */
    
  }
  
#endif

}
