/*  
 *   Sartoris system call header
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


/* sartoris system calls */

#ifndef SYSCALL
#define SYSCALL

#include <sartoris/kernel.h>

#ifdef CPLUSPLUS
extern "C" {
#endif

/* multitasking */
int create_task(int address, struct task *tsk, int *src, unsigned int init_size);
int destroy_task(int task_num);
int get_current_task(void);

/* threading */
int create_thread(int id, struct thread *thr);
int destroy_thread(int id);
int run_thread(int id);
int set_thread_run_perm(int thread, int perm);
int set_thread_run_mode(int priv, int mode);
int get_current_thread(void);

/* paging */
int page_in(int task, void *linear, void *physical, int level, int attrib);
int page_out(int task, void *linear, int level);
int flush_tlb(void);
int get_page_fault(struct page_fault *pf);

/* interrupt handling */
int create_int_handler(int number, int thread, int nesting, int priority);
int destroy_int_handler(int number, int thread);
int ret_from_int(void);
int get_last_int(void);

/* message passing */
int open_port(int port, int priv, int mode);
int close_port(int port);
int set_port_perm(int port, int task, int perm);
int set_port_mode(int port, int priv, int mode);
int send_msg(int to_address, int port, void *msg);
int get_msg(int port, void *msg, int *id);
int get_msg_count(int port);

/* memory sharing */
int share_mem(int target_task, void *addr, int size, int perms);
int claim_mem(int smo_id);
int read_mem(int smo_id, int off, int size, void *dest);
int write_mem(int smo_id, int off, int size, void *src);
int pass_mem(int smo_id, int target_task);
int mem_size(int smo_id);

#ifdef _METRICS_
int get_metrics(struct sartoris_metrics *m);
#endif

#ifdef _SOFTINT_ 
int run_thread_int(int id, void *eip, void *stack);
#endif

#ifdef CPLUSPLUS
}
#endif

#endif
