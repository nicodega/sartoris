/*  
 *   Sartoris system call header
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */


/* sartoris system calls */

#ifndef SYSCALL
#define SYSCALL

#include <sartoris/kernel.h>
#include <sartoris/target/reg-calls.h>
#ifdef _METRICS_
#include <sartoris/metrics.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
	
/* multitasking */
int ARCH_FUNC_ATT2 create_task(int id, struct task *tsk);
int ARCH_FUNC_ATT3 init_task(int task, int *src, unsigned int size);
int ARCH_FUNC_ATT1 destroy_task(int task_num);
int ARCH_FUNC_ATT0 get_current_task(void);

/* threading */
int ARCH_FUNC_ATT2 create_thread(int id, struct thread *thr);
int ARCH_FUNC_ATT1 destroy_thread(int id);
int ARCH_FUNC_ATT1 run_thread(int id);
int ARCH_FUNC_ATT2 set_thread_run_perms(int thr_id, struct permissions *perms);
int ARCH_FUNC_ATT3 set_thread_run_mode(int thr_id, int priv, enum usage_mode mode);
int ARCH_FUNC_ATT0 get_current_thread(void);

void idle_cpu();

/* paging */
int ARCH_FUNC_ATT5 page_in(int task, void *linear, void *physical, int level, int attrib);
int ARCH_FUNC_ATT3 page_out(int task, void *linear, int level);
int ARCH_FUNC_ATT0 flush_tlb(void);
int ARCH_FUNC_ATT1 get_page_fault(struct page_fault *pf);
int ARCH_FUNC_ATT1 grant_page_mk(void *physical);

/* interrupt handling */
int ARCH_FUNC_ATT4 create_int_handler(int number, int thread, int nesting, int priority);
int ARCH_FUNC_ATT2 destroy_int_handler(int number, int thread);
int ARCH_FUNC_ATT0 ret_from_int(void);
int ARCH_FUNC_ATT1 get_last_int(unsigned int *error_code);
void * ARCH_FUNC_ATT0 get_last_int_addr();
int ARCH_FUNC_ATT0 pop_int();
int ARCH_FUNC_ATT1 push_int(int number);
int ARCH_FUNC_ATT0 resume_int();
int ARCH_FUNC_ATT3 run_thread_int(int id, void *eip, void *stack);

/* message passing */
int ARCH_FUNC_ATT3 open_port(int port, int priv, enum usage_mode mode);
int ARCH_FUNC_ATT1 close_port(int port);
int ARCH_FUNC_ATT2 set_port_perm(int port, struct permissions *perms);
int ARCH_FUNC_ATT3 set_port_mode(int port, int priv, enum usage_mode mode);
int ARCH_FUNC_ATT3 send_msg(int to_address, int port, void *msg);
int ARCH_FUNC_ATT3 get_msg(int port, void *msg, int *id);
int ARCH_FUNC_ATT4 get_msgs(int port, int *msgs, int *ids, int maxlen);
int ARCH_FUNC_ATT1 get_msg_count(int port);
int ARCH_FUNC_ATT3 get_msg_counts(int *ports, int *counts, int len);

/* memory sharing */
int ARCH_FUNC_ATT4 share_mem(int target_task, void *addr, int size, int perms);
int ARCH_FUNC_ATT1 claim_mem(int smo_id);
int ARCH_FUNC_ATT4 read_mem(int smo_id, int off, int size, void *dest);
int ARCH_FUNC_ATT4 write_mem(int smo_id, int off, int size, void *src);
int ARCH_FUNC_ATT2 pass_mem(int smo_id, int target_task);
int ARCH_FUNC_ATT1 mem_size(int smo_id);

/* tracing */
int ARCH_FUNC_ATT2 ttrace_begin(int thr_id, int task_id);
int ARCH_FUNC_ATT2 ttrace_end(int thr_id, int task_id);
int ARCH_FUNC_ATT4 ttrace_reg(int thr_id, int reg, void *value, int set);
int ARCH_FUNC_ATT4 ttrace_mem_read(int thr_id, void *src, void *dst, int size);
int ARCH_FUNC_ATT4 ttrace_mem_write(int thr_id, void *src, void *dst, int size);

/* Errors */
int ARCH_FUNC_ATT0 last_error();

/* Events */
#define SARTORIS_EVT_MSG            1
#define SARTORIS_EVT_PORT_CLOSED    2
#define SARTORIS_EVT_INT            3
#define SARTORIS_EVT_INTS           4

struct evt_msg
{
    int evt;
    int id;
    int param;
    int param2;
}  __attribute__ ((__packed__));

struct ints_evt_param{
	unsigned int mask;
	int base;
};

int ARCH_FUNC_ATT3 evt_set_listener(int thread, int port, int interrupt);
int ARCH_FUNC_ATT3 evt_wait(int id, int evt, int evt_param);
int ARCH_FUNC_ATT3 evt_disable(int id, int evt, int evt_param);

#ifdef _METRICS_
int ARCH_FUNC_ATT1 get_metrics(struct sartoris_metrics *m);
#endif

#ifdef __cplusplus
}
#endif

#endif
