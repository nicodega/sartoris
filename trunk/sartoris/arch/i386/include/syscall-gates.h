/*  
 *   Sartoris system call gates header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


#ifndef SYSCALL_GATES
#define SYSCALL_GATES

#include <sartoris/types.h>

extern int create_task_c(int, struct task*, int *initm, unsigned int init_size);
extern int destroy_task_c(int);
extern int get_current_task_c(void);

extern int create_thread_c(int, struct thread*);
extern int destroy_thread_c(int);
extern int set_thread_run_perm_c(int thread, int perm);
extern int set_thread_run_mode_c(int priv, int mode);
extern int run_thread_c(int);
extern int get_current_thread_c(int);

extern int page_in_c(int task, void *linear, void *physical, int level, int attrib);
extern int page_out_c(int task, void *linear, int level);
extern int flush_tlb_c(void);
extern void *get_page_fault_c(void);

extern int create_int_handler_c(int number, int thread, int nesting, int priority);
extern int destroy_int_handler_c(int number, int thread);
extern int ret_from_int_c(void);
extern int get_last_int_c(void);

extern int open_port_c(int port, int priv, int mode);
extern int close_port_c(int port);
extern int set_port_perm_c(int port, int task, int perm);
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

#endif
