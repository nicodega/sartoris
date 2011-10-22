/*  
 *   Sartoris system call gates header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */


#ifndef SYSCALL_GATES
#define SYSCALL_GATES

#include <sartoris/kernel.h>
#include <sartoris/types.h>

extern int create_task_c(int id, struct task*);
extern int init_task_c(int id, int *initm, unsigned int init_size);
extern int destroy_task_c(int);
extern int get_current_task_c(void);

extern int create_thread_c(int, struct thread*);
extern int destroy_thread_c(int);
extern int set_thread_run_perms_c(int thr_id, struct permissions *perm);
extern int set_thread_run_mode_c(int thr_id, int priv, int mode);
extern int run_thread_c(int id);
extern int run_thread_int_c(int id, void *eip, void *stack);
extern int get_current_thread_c(int);

extern int page_in_c(int task, void *linear, void *physical, int level, int attrib);
extern int page_out_c(int task, void *linear, int level);
extern int flush_tlb_c(void);
extern void *get_page_fault_c(void);
extern int grant_page_mk_c(void *physical);

extern int create_int_handler_c(int number, int thread, int nesting, int priority);
extern int destroy_int_handler_c(int number, int thread);
extern int ret_from_int_c(void);
extern int get_last_int_c(unsigned int *error_code);
extern void *get_last_int_addr_c();

extern int open_port_c(int port, int priv, int mode);
extern int close_port_c(int port);
extern int set_port_perm_c(int port, struct permissions *perms);
extern int set_port_mode_c(int port, int priv, int mode);
extern int send_msg_c(int, int, int*);
extern int get_msg_c(int, int*, int*);
extern int get_msg_count_c(int);

extern int share_mem_c(int, int, int, int);
extern int claim_mem_c(int);
extern int read_mem_c(int, int, int, int*);
extern int write_mem_c(int, int, int, int*);
extern int pass_mem_c(int, int);
extern int mem_size_c(int);

extern int ret_from_int_c(void);

extern int push_int_c(int);
extern int pop_int_c(void);
extern int resume_int_c(void);

extern int last_error_c(void);

extern int ttrace_begin_c(int thr_id, int task_id);
extern int ttrace_end_c(int thr_id, int task_id);
extern int ttrace_reg_c(int thr_id, int reg, void *value, int set);
extern int ttrace_mem_read_c(int thr_id, void *src, void *dst, int size);
extern int ttrace_mem_write_c(int thr_id, void *src, void *dst, int size);

#ifdef _METRICS_
extern int get_metrics_c(void);
#endif

#endif
