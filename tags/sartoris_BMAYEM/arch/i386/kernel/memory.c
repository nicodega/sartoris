/*  
 *   Sartoris microkernel i386-dependent memory-handling functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar
 */

#include "sartoris/cpu-arch.h"
#include "cpu-arch-inline.h"
#include "kernel-arch.h"
#include "i386.h"
#include "paging.h"

#ifdef PAGING
extern pd_entry *tsk_pdb[MAX_TSK];
#endif

#ifndef PAGING

/* the simple version: paging is disabled */

int arch_cpy_to_task(int task, char* src, char* dst, unsigned int len) {
  
  arch_mem_cpy_bytes(MAKE_KRN_PTR(src), MAKE_KRN_SHARED_PTR(task, dst), len);
  
  return len;
  
}

int arch_cpy_from_task(int task, char*src, char* dst, unsigned int len) {

  arch_mem_cpy_bytes(MAKE_KRN_SHARED_PTR(task, src), MAKE_KRN_PTR(dst), len);

  return len;
  
}

#else /* the kamikaze version: paging is enabled */

/* we have to temporary map the dst
   area of memory in order to copy it */

/* in this incarnation, we have num_task one-page slots in
   the linear space. */

/* yes, it's just _linear_ space, of which the kernel has
   at its disposal the first 8 megabytes, we're using
   some room just beyond the 2nd megabyte mark. */

/* this functions are sometimes called within critical blocks.
   when we provoke a page fault, we must return informing how
   many bytes were copied. hence the calling function resumes
   control after the page fault and might repeat any pertinent
   verifications
   
   (because the atomicity has been broken!) */

/* this is so freaking sound it makes me wanna cry! */

int arch_cpy_to_task(int task, char* src, char* dst, unsigned int len) {
  char *mapped_dst;
  unsigned int i, j;  /* i: index for curr_task, j: index for task */
  int import_res;
  int local_res;

  src = (char*)MAKE_KRN_PTR(src);
  
  mapped_dst = AUX_PAGE_SLOT(curr_thread); 
  
  j = (unsigned int)dst % PG_SIZE;
  
  dst = (char*) PG_ADDRESS(dst); /* this moves dst to the largest
                            page boundary not greater
                            than itself */
  
  local_res = verify_present(src, false);
  
  import_res = import_page(task, dst);

  for(i = 0; i < len; i++) {
    
    /* if page not present, bail out! */
     if (((unsigned int)src + i) % PG_SIZE == 0) {
       local_res = verify_present(&src[i], false);
     }
     
     if (local_res == SUCCESS) {
     
       if (import_res == SUCCESS) {
	 
	  mapped_dst[j++] = src[i];
	 
       } else { /* issue page fault for target (imported) task */
	 
	 last_page_fault.task_id = task;
	 last_page_fault.thread_id = curr_thread;
	 last_page_fault.linear = dst;
       
	 arch_issue_page_fault();
	 break;
       }

     } else { /* issue page fault for source (current) task */
       
       last_page_fault.task_id = curr_task;
       last_page_fault.thread_id = curr_thread;
       last_page_fault.linear = &src[i];
       
       arch_issue_page_fault();
       break;
     }
    
     if (j % PG_SIZE == 0 && i < len) {
       
       dst+=PG_SIZE;
       import_res = import_page(task, dst);
       
       j = 0;
     }
     
  } 
  
  return i;
  
}

int arch_cpy_from_task(int task, char* src, char* dst, unsigned int len) {
  char *mapped_src;
  unsigned int i, j;  /* i: index for curr_task, j: index for task */
  int import_res;
  int local_res;

  dst = MAKE_KRN_PTR(dst);
  
  mapped_src = AUX_PAGE_SLOT(curr_thread); 
  
  j = (unsigned int)src % PG_SIZE;
  
  src = (char*) PG_ADDRESS(src); /* this moves src to the largest
                            page boundary not greater
                            than itself */
  
  local_res = verify_present(dst, true);

  import_res = import_page(task, src); // map source page on the one-slot window

  for(i = 0; i < len; i++) {

    /* if page not present, bail out! */
     if (((unsigned int)dst + i) % PG_SIZE == 0) {
       local_res = verify_present(&dst[i], true);
     }
     
     if (local_res == SUCCESS) {
     
       if (import_res == SUCCESS) {
	 
	  dst[i] = mapped_src[j++];
	 
       } else { /* issue page fault for source (imported) task */
	 
	 last_page_fault.task_id = task;
	 last_page_fault.thread_id = curr_thread;
	 last_page_fault.linear = &src[i];
       
	 arch_issue_page_fault();
	 break;
       }

     } else { /* issue page fault for target (current) task */
       
       last_page_fault.task_id = curr_task;
       last_page_fault.thread_id = curr_thread;
       last_page_fault.linear = dst;
       
       arch_issue_page_fault();
       break;
     }
    
     if (j % PG_SIZE == 0 && i < len) {
       
       src+=PG_SIZE;
       import_res = import_page(task, src);
       
       j = 0;
     }
     
  } 
  
  return i;
  
}

#endif

/* to do: write this in assembly. */

/* these copy functions should work fine
   when the memory areas to copy overlap. */

void arch_mem_cpy_words(int* src, int* dst, unsigned int len) {
    unsigned int i;
 
    if (src > dst) {    
        for (i=0; i<len; i++) { dst[i]=src[i]; }
    } else {
	for (i=len-1; i>=0; i--) { dst[i]=src[i]; }
    }	
}

void arch_mem_cpy_bytes(char *src, char *dst, unsigned int len) {
    unsigned int i;
    int *wsrc, *wdst;
   
    if (src > dst) { 

        wsrc = (int*) src;
	wdst = (int*) dst;
	
	for (i=0; i<len/4; i++) { wdst[i]=wsrc[i]; }
        for (i=len/4; i<(len/4)+(len%4); i++) {dst[i] = src[i]; }
    
    } else {

	wsrc = (int*) (src-3);
	wdst = (int*) (dst-3);

	for (i=(len/4)-1; i>=0; i--) { wdst[i]=wsrc[i]; }
        for (i=len%4; i>=0; i--) { dst[i] = src[i]; }

    }
}
