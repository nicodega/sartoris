/*  
 *   Sartoris microkernel i386 thread support functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "kernel-arch.h"
#include "tss.h"
#include "descriptor.h"
#include "paging.h"

#ifdef PAGING
extern pd_entry *tsk_pdb[MAX_TSK];   /* needed to quickly */
#endif

extern struct tss thr_tss[MAX_THR];  /* update cr3 ;)     */

int arch_create_thread(int id, int priv, struct thread* thr) {
  
  build_tss(id, thr->task_num, priv, thr->ep, thr->stack);
  init_tss_desc(id);
  
  return 0;
}

int arch_destroy_thread(int id, struct thread* thr) {
 
  inv_tss_desc(id);

  return 0;
}

int arch_run_thread(int id) {
  unsigned int tsk_sel[2];

#ifdef PAGING
  int tsk_num;
  pd_entry *page_dir_base;
  
  tsk_num=threads[id].task_num;
  page_dir_base = tsk_pdb[tsk_num];

  if (page_dir_base == NULL) {
    return FAILURE;
  }
  
  thr_tss[id].cr3 = (unsigned int)page_dir_base;

#endif
  
  tsk_sel[0] = 0;
  tsk_sel[1] = ((GDT_TSS + id) << 3);
  
  __asm__ ("ljmp *%0" : : "m" (*tsk_sel));
  
  return SUCCESS;
}
