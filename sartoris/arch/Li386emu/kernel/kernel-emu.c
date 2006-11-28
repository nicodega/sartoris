#include <sartoris/kernel.h>
#include <sartoris/cpu-arch.h>
#include <sartoris/scr-print.h>
#include <emulator.h>
#include <kernel-emu.h>

struct emu_thread ethreads[MAX_THR];

int global_mem_base;
int good_flags;

void arch_init_cpu(void) {
  struct task tsk;
  struct thread thr;
  
  emu_set_thread_state_base(ethreads);
  
  global_mem_base = (int) emu_mem_base();
  
  asm ( "pushf \n"
        "pop %0" : "=m" (good_flags) : );
  
  tsk.mem_adr = 0x700000;
  tsk.size = 0x100000-0x1;
  tsk.type = SRV_TASK;
  
  create_task(INIT_TASK_NUM, &tsk, NULL, 0);
  
  thr.task_num=INIT_TASK_NUM;
  thr.type = SRV_TASK | RUNNABLE;
  thr.irq = 0;
  thr.ep = 0x00000000;
  thr.stack = 0x100000-0x4;
  
  create_thread(INIT_THREAD_NUM, &thr);
    
  
  //emu_output("init thread register layout:\n\n");
  //emu_print_active_regs();
  //emu_output("\nenabling emu-interrupts and begginig simulation...");
  emu_int_enable();
  
  emu_output("\nok. init thread activated NOW!");
  
  run_thread(INIT_THREAD_NUM);
  
  while(1);
}

int arch_create_task(int task_num, struct task* tsk) {
  return 0;
}

int arch_destroy_task(int num) {
  return 0;
}

int arch_create_thread(int id, struct thread* thr) {
  build_emu_thr(&ethreads[id], thr);
  return 0;
}

int arch_destroy_thread(int id, struct thread* thr) {

}
int arch_run_thread(int id) {
  emu_dispatch(id);
}

void arch_mem_cpy(int* src, int* dst, int l) {
int i;
  
  if(src > dst) {
    for (i=0; i<l; i++) { dst[i]=src[i]; }
  } else {
    for (i=l-1; i>=0; i--) { dst[i]=src[i]; }
  }
}

void arch_dump_cpu(void) {

}

int arch_cli(void) {
  int x;
  
  x = emu_int_state();
  if (x) emu_int_disable();
  return x;
}

void arch_sti(int x) {
  if (x) emu_int_enable();
}


void build_emu_thr(struct emu_thread *emu_thr, struct thread *thr) {
  int emu_thr_base;
  
  emu_thr_base = global_mem_base + tasks[thr->task_num].mem_adr; 
  
  emu_thr->eax = emu_pid();
  emu_thr->ebx = 0x111;
  emu_thr->ecx = 0x112;
  emu_thr->edx = 0x113;
  emu_thr->esi = 0x114;
  emu_thr->edi = 0x115;
  emu_thr->ebp = 0x116;
  emu_thr->esp = emu_thr_base + thr->stack;
  emu_thr->eflags = good_flags;
  emu_thr->eip = emu_thr_base + thr->ep;
  emu_thr->int_enabled = 0;
  emu_thr->priv_level = 2;
  
}


void k_scr_init(void) {

}

void k_scr_newLine(void) {

}

void k_scr_moveUp(void) {

}

void k_scr_print(char* text, byte att) {

}

void k_scr_hex(int n, byte att) {

}

void k_scr_clear(void) {

}
