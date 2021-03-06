/*  
 *   Sartoris cpu-arch dependent functions header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */


/* This are the arch-dependent functions needed to support the kernel */

/* IMPORTANT NOTE: all functions should return -1 on failure! */

#ifndef CPUARCH
#define CPUARCH

#include <sartoris/kernel.h>
#include <reg-calls.h>
#include <cpu-arch-inline.h>
#include "lib/containers.h"

void arch_init_cpu(void);

int arch_create_task(int id, struct task *tsk);
int arch_destroy_task(int id);

int arch_create_thread(int id, int priv, struct thread *thr);
int arch_destroy_thread(int id, struct thread* thr);
int ARCH_FUNC_ATT1 arch_run_thread(int id);
int ARCH_FUNC_ATT3 arch_run_thread_int(int id, void *eip, void *stack);
int arch_is_soft_int();

int arch_create_int_handler(int number);
int arch_destroy_int_handler(int number);

int arch_page_in(int task, void *linear, void *physical, int level, int attrib);
int arch_page_out(int task, void *linear, int level);
int arch_kernel_pf(void *linear);
int arch_grant_page_mk(void *physical);

int arch_ttrace_begin(int thr_id);
void arch_ttrace_end(int thr_id);
int arch_ttrace_reg_size(int reg);
int arch_ttrace_set_reg(int thr_id, int reg, void *value);
int arch_ttrace_get_reg(int thr_id, int reg, void *value);
int arch_ttrace_get_regs(int thr_id, void *ptr_regs);
int arch_ttrace_set_regs(int thr_id, void *ptr_regs);

void arch_mem_cpy_words(int *src, int *dst, unsigned int len);
void arch_mem_cpy_bytes(char *src, char *dst, unsigned int len);

int arch_test_and_set(int *x, int value);

void arch_caps_init();

#ifdef PAGING

/*
This function must request a page from the operating system. It *must*
do so by issuing a Page Fault interrupt.
It must return a linear sartoris address already mapped.
*/
int arch_request_page(void *laddr);

void arch_init_dynmem();

/*
This function must return a page to the operating system. It *must*
do so by issuing a Page Fault interrupt.
if we could return the page to the OS, it must return 1, 0 otherwise.
*/
int arch_return_page(void *laddr);

/*
Get physical address for the page being freed by arch_return_page.
*/
void *arch_get_freed_physical();

/*
Returns 0 if page granting/return finished.
*/
int arch_req_pages();

int arch_get_perms(int task, struct permissions *localperms, struct permissions *p);
unsigned int *arch_map_perms(int task, struct permissions *perms, unsigned int pos);

#endif

/* 
  The following two return the number of bytes copied.
  if paging is enabled, and a page fault (in either task)
  occurs, the function will return the number of bytes
  copied, and will do it after the page fault has been
  served (i.e., it should detect it has just faulted,
  and abort informing how much it copied).
*/
int arch_cpy_to_task(int task, char* src, char* dst, unsigned int len, int x, int trace);
int arch_cpy_from_task(int task, char* src, char* dst, unsigned int len, int x);

void arch_dump_cpu(void);

#ifndef HAVE_INL_CLI
int arch_cli(void);   
#endif

#ifndef HAVE_INL_STI
void arch_sti(int x);
#endif

#ifndef HAVE_INL_GET_PAGE_FAULT
void *arch_get_page_fault(void);
#endif

#ifndef HAVE_INL_ARCH_EVENT_RAISE
void *arch_event_raise(void);
#endif

#ifndef HAVE_INL_ISSUE_PAGE_FAULT
void arch_issue_page_fault(void);
#endif

#ifndef HAVE_INL_IDLE_CPU
void arch_idle_cpu(void);
#endif

#ifndef MAKE_KRN_PTR
# define MAKE_KRN_PTR(x) ((void*)x)
#endif

#ifndef MAKE_KRN_SHARED_PTR
# define MAKE_KRN_SHARED_PTR(t, x) ((void*)x)
#endif

#endif



