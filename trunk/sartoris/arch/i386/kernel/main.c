/*  
 *   Sartoris microkernel i386-dependent main file
 *   
 *   Copyright (C) 2002, 2003, 2004, 
 *         Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar      
 */

#include "sartoris/cpu-arch.h"
#include "kernel-arch.h"
#include "i386.h"
#include "syscall-gates.h"
#include "interrupt.h"
#include "descriptor.h"
#include "paging.h"

/* init_cpu initializes de system gdt (desc 0-3 are system desc: [dummy, 
 * kcode, kdata, himem]) and creates the init service.
 */

void create_init_task(void);
void create_syscall_gates(void);

void arch_init_cpu(void) 
{
	/* remap the interrupts */
	reprog_pics();

	/* initialize gdt                                     */
	init_desc_tables();

	/* initialize system call gates                       */
	create_syscall_gates();

	/* idt initialization */
	init_interrupts();

	create_init_task();  
  
#ifdef PAGING
	init_paging();
#endif

	/* the system is almost up! the rest is done back in the initalize_kernel 
		function, namely actually firing the init thread. */
}

void create_syscall_gates() 
{
	/* second parameter is required priv. level to invoke */
	hook_syscall(0, 1, &create_task_c, 4);
	hook_syscall(1, 1, &destroy_task_c, 1);
	hook_syscall(2, 3, &get_current_task_c, 0);

	hook_syscall(3, 1, &create_thread_c, 2);
	hook_syscall(4, 1, &destroy_thread_c, 1);
	hook_syscall(5, 3, &set_thread_run_perm_c, 2);
	hook_syscall(6, 3, &set_thread_run_mode_c, 2);
	hook_syscall(7, 3, &run_thread_c, 1);
	hook_syscall(8, 3, &get_current_thread_c, 0);

	hook_syscall(9, 1, &page_in_c, 5);
	hook_syscall(10, 1, &page_out_c, 3);
	hook_syscall(11, 1, &flush_tlb_c, 0);
	hook_syscall(12, 2, &get_page_fault_c, 1);

	hook_syscall(13, 1, &create_int_handler_c, 4);
	hook_syscall(14, 1, &destroy_int_handler_c, 2);
	hook_syscall(15, 2, &ret_from_int_c, 0);
	hook_syscall(16, 2, &get_last_int_c, 0);

	hook_syscall(17, 3, &open_port_c, 2);
	hook_syscall(18, 3, &close_port_c, 1);
	hook_syscall(19, 3, &set_port_perm_c, 3);
	hook_syscall(20, 3, &set_port_mode_c, 3);
	hook_syscall(21, 3, &send_msg_c, 3);
	hook_syscall(22, 3, &get_msg_c, 3);
	hook_syscall(23, 3, &get_msg_count_c, 1);

	hook_syscall(24, 3, &share_mem_c, 4);
	hook_syscall(25, 3, &claim_mem_c, 1);
	hook_syscall(26, 3, &read_mem_c, 4);
	hook_syscall(27, 3, &write_mem_c, 4);
	hook_syscall(28, 3, &pass_mem_c, 2);
	hook_syscall(29, 3, &mem_size_c, 1);
}

void create_init_task() 
{
	struct task tsk;
	struct thread thr;
	int i;

	/* ok, now the init task and thread */

#ifdef PAGING                              /* if we are paging, do not allow the */
	tsk.size = INIT_SIZE;			   /* init thread to  overwrite its own  */
#else                                      /* page tables                        */
	tsk.size = INIT_SIZE - 1;
#endif
  
#ifdef PAGING                              /* if we are paging, the address is   */
	tsk.mem_adr = (void*)USER_OFFSET;        /* the usual linear address           */
#else
	tsk.mem_adr = (void*)INIT_OFFSET;        /* if we are not paging, give the     */
#endif                                     /* physical address directly          */

	tsk.priv_level = 0;

	thr.task_num = INIT_TASK_NUM;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 1;
	thr.ep = 0x00000000;

  /* if we are paging, don't put the stack over bootinfo */
#ifdef PAGING  
	thr.stack = (void*)(INIT_SIZE - BOOTINFO_SIZE - 0x4);
#else
	thr.stack = (void*)(INIT_SIZE - BOOTINFO_SIZE - 0x4);
#endif

  if (create_task(INIT_TASK_NUM, &tsk, NULL, 0) < 0) while(1);
  if (create_thread(INIT_THREAD_NUM, &thr) < 0) while(1);
}
