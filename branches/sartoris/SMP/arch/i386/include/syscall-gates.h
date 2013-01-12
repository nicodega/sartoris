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
#include <sartoris/target/reg-calls.h>

extern int ARCH_FUNC_ATT2 create_task_c(int id, struct task*);
extern int ARCH_FUNC_ATT3 init_task_c(int id, int *initm, unsigned int init_size);
extern int ARCH_FUNC_ATT1 destroy_task_c(int);
extern int ARCH_FUNC_ATT0 get_current_task_c();

extern int ARCH_FUNC_ATT2 create_thread_c(int, struct thread*);
extern int ARCH_FUNC_ATT1 destroy_thread_c(int);
extern int ARCH_FUNC_ATT2 set_thread_run_perms_c(int thr_id, struct permissions *perm);
extern int ARCH_FUNC_ATT3 set_thread_run_mode_c(int thr_id, int priv, int mode);
extern int ARCH_FUNC_ATT1 run_thread_c(int id);
extern int ARCH_FUNC_ATT3 run_thread_int_c(int id, void *eip, void *stack);
extern int ARCH_FUNC_ATT1 get_current_thread_c(int);

extern void ARCH_FUNC_ATT0 idle_cpu_c();

extern int ARCH_FUNC_ATT5 page_in_c(int task, void *linear, void *physical, int level, int attrib);
extern int ARCH_FUNC_ATT3 page_out_c(int task, void *linear, int level);
extern int ARCH_FUNC_ATT0 flush_tlb_c();
extern int ARCH_FUNC_ATT1 get_page_fault_c(struct page_fault *pf);
extern int ARCH_FUNC_ATT1 grant_page_mk_c(void *physical);

int map_hard_int_c(int hard_int, int vector, ...);
extern int ARCH_FUNC_ATT4 create_int_handler_c(int number, int thread, int nesting, int priority);
extern int ARCH_FUNC_ATT2 destroy_int_handler_c(int number, int thread);
extern int ARCH_FUNC_ATT1 ret_from_int_c();
extern int ARCH_FUNC_ATT1 get_last_int_c(unsigned int *error_code);
extern void * ARCH_FUNC_ATT0 get_last_int_addr_c();
extern void ARCH_FUNC_ATT0 eoi_c();

extern int ARCH_FUNC_ATT3 open_port_c(int port, int priv, int mode);
extern int ARCH_FUNC_ATT1 close_port_c(int port);
extern int ARCH_FUNC_ATT2 set_port_perm_c(int port, struct permissions *perms);
extern int ARCH_FUNC_ATT3 set_port_mode_c(int port, int priv, int mode);
extern int ARCH_FUNC_ATT3 send_msg_c(int, int, int*);
extern int ARCH_FUNC_ATT3 get_msg_c(int, int*, int*);
extern int ARCH_FUNC_ATT1 get_msg_count_c(int);
extern int ARCH_FUNC_ATT4 get_msgs_c(int port, int *msgs, int *ids, int maxlen);
extern int ARCH_FUNC_ATT3 get_msg_counts_c(int *ports, int *counts, int len);

extern int ARCH_FUNC_ATT4 share_mem_c(int, int, int, int);
extern int ARCH_FUNC_ATT1 claim_mem_c(int);
extern int ARCH_FUNC_ATT4 read_mem_c(int, int, int, int*);
extern int ARCH_FUNC_ATT4 write_mem_c(int, int, int, int*);
extern int ARCH_FUNC_ATT2 pass_mem_c(int, int);
extern int ARCH_FUNC_ATT1 mem_size_c(int);

extern int ARCH_FUNC_ATT1 push_int_c(int);
extern int ARCH_FUNC_ATT0 pop_int_c();
extern int ARCH_FUNC_ATT0 resume_int_c();

extern int ARCH_FUNC_ATT0 last_error_c();

extern int ARCH_FUNC_ATT2 ttrace_begin_c(int thr_id, int task_id);
extern int ARCH_FUNC_ATT2 ttrace_end_c(int thr_id, int task_id);
extern int ARCH_FUNC_ATT4 ttrace_reg_c(int thr_id, int reg, void *value, int set);
extern int ARCH_FUNC_ATT4 ttrace_mem_read_c(int thr_id, void *src, void *dst, int size);
extern int ARCH_FUNC_ATT4 ttrace_mem_write_c(int thr_id, void *src, void *dst, int size);

extern int ARCH_FUNC_ATT3 evt_set_listener_c(int thread, int port, int interrupt);
extern int ARCH_FUNC_ATT2 evt_wait_c(int id, int evt);
extern int ARCH_FUNC_ATT3 evt_disable_c(int id, int evt, int evt_param);

#ifdef _METRICS_
extern int ARCH_FUNC_ATT0 get_metrics_c();
#endif

#endif
