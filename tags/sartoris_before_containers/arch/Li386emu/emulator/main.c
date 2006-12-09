#include <asm/sigcontext.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#include "syscall.h"
#include "emu_internals.h"

/* ======== this defines our virtual cpu ==================== */

/* our CPL                                                    */
unsigned char priv_level;                

/* our IOP                                                    */
unsigned char io_priv_level;

/* our task register                                          */
int active_thread_num;

/* entry point for syscalls                                   */
/* (-1) means no such syscall                                 */
unsigned int syscall_table[MAX_SCA]; 

/* priv level for syscall                                     */                                
char syscall_priv[MAX_SCA]; 

/* our very own IF                                            */
char int_enabled;               

/* which IRQ lines are raised                                 */
char int_req[MAX_IRQ];

/* priv level for soft irqs                                   */
char int_priv[MAX_IRQ];         

/* our IDT, each entry is a thread number */
/* (-1) means not enabled                                     */         
unsigned int int_table[MAX_IRQ];

/* irq mode ; IRQ_DRV or IRQ_HDL */
int int_mode[MAX_IRQ];

/* emulated memory buffer                                     */                               
char mem_space[EMU_MEM_SIZE];  

/* the offset of a threads table                              */
struct emu_thread *thread_state_table;

/* the interrupts stack                                       */
int num_nested_thr;
int nested_thr[EMU_MAX_NESTED];

/* ioports array */

void (*ports[EMU_NUM_PORTS])(int);

static int itzd=0;

/* ========================================================== */

int main() {
  fprintf(stdout, "\nLi386emu version 0.01 booting.\n");
  fflush(stdout);
  emu_startup();
  emu_bootstrap();
  initialize_kernel();
  while(1);
  return 0;

}

void emu_startup() {
  int i;
  
  priv_level = io_priv_level = 0;
  active_thread_num=-1;
  
  for (i=0; i<MAX_SCA; i++) {
    syscall_table[i] = -1;
  }
  
  int_enabled = 0;
  
  for (i=0; i<MAX_IRQ; i++) {
    int_req[i] = 0;
    int_table[i] = -1;
  }
  
  for(i=0; i<EMU_NUM_PORTS; i++) {
    ports[i] = NULL;
  }

  num_nested_thr = 0;
  
  signal(EMU_CLOCK_SIGNAL, sigtimer_handler);
  signal(EMU_SYSCALL_SIGNAL, syscall_handler);
  
}

void emu_trampoline() { 
  static int i=0;
  struct sigcontext *sc;
  int *params;
  int j;
  int *s;
  
  switch (emu_action) {    
  case ACT_CLOCK:
    
    break;
    
  case ACT_SYSCALL:
    sc = (struct sigcontext*) (entry_esp + 8);
    params = (void*)sc->edi;    
    switch ((int)sc->esi) {
    case CREATE_TASK:
      sc->eax=create_task((int)params[0], (struct task*)params[1], (int*)params[2], (int)params[3]);
      break;
    case DESTROY_TASK:
      sc->eax=destroy_task((int)params[0]);
      break;
    case CREATE_THREAD:
      sc->eax=create_thread((int)params[0], (struct thread*)params[1]);
      break;
    case DESTROY_THREAD:
      sc->eax=destroy_thread((int)params[0]);
      break;
    case RUN_THREAD:
       sc->eax=run_thread((int)params[0]);
      break;
    case SEND_MSG:
      sc->eax=send_msg((int)params[0], (int)params[1], (void*)params[2]);
      break;
    case GET_MSG:
      sc->eax=get_msg((int)params[0], (void*)params[1], (int*)params[2]);
      break;
    case GET_MSG_COUNT:
      sc->eax=get_msg_count((int)params[0]);
      break;
    case SHARE_MEM:
      sc->eax=share_mem((int)params[0], (void*)params[1], (int)params[2], (int)params[3]);
      break;
    case CLAIM_MEM:
      sc->eax=claim_mem((int)params[0]);
      break;
    case READ_MEM:
      sc->eax=read_mem((int)params[0], (int)params[1], (int)params[2], (void*)params[3]);
      break;
    case WRITE_MEM:
      sc->eax=write_mem((int)params[0], (int)params[1], (int)params[3], (void*)params[3]);
      break;
    case PASS_MEM:
      sc->eax=pass_mem((int)params[0], (int)params[1]);
      break;
    case SET_THREAD_RUN_PERMS:
      sc->eax=set_thread_run_perms((int)params[0], (int)params[1]);
      break;
    case SET_THREAD_RUN_MODE:
      sc->eax=set_thread_run_perms((int)params[0]);
      break;
    case SET_PORT_MODE:
      sc->eax=set_port_mode((int)params[0], (int)params[1]);
      break;
    case SET_PORT_PERMS:
      sc->eax=set_port_perms((int)params[0], (int)params[1], (int)params[2]);
      break;
    case CLOSE_PORT:
      sc->eax=close_port((int)params[0]);
      break;
    case OPEN_PORT:
      sc->eax=open_port((int)params[0], (int)params[1]);
      break;
      
    case 100:
      fprintf(stdout, (char *) params[0]); 
      fflush(stdout);
      break;
    }
    
    signal(EMU_SYSCALL_SIGNAL, syscall_handler);
    break;
    
  case ACT_IO:
    
    break;

  }
  
  return;
}

void emu_set_thread_state_base(struct emu_thread *t) {
  thread_state_table=t;
}
 
int emu_dispatch(int thread) {
  struct emu_thread *thr;
  struct sigcontext *sc;
  
  if (!itzd) {
    itzd=1;
    emu_begin_simulation(thread);
    /* unreachable */
  }
  
  sc = (struct sigcontext*)(entry_esp + 8);
  
  emu_int_disable();
  
  /* save old context */
  if (active_thread_num != -1) {
    
    thr = &thread_state_table[active_thread_num];
    thr->eax = sc->eax;
    thr->ebx = sc->ebx;
    thr->ecx = sc->ecx;
    thr->edx = sc->edx;
    thr->esi = sc->esi;
    thr->edi = sc->edi;
    thr->esp = sc->esp;
    thr->ebp = sc->ebp;
    thr->eip = sc->eip;
    thr->eflags = sc->eflags;
    thr->int_enabled = int_enabled;
    thr->priv_level = priv_level;
  }
  
  
  thr = &thread_state_table[thread];
  
  sc->eax = thr->eax;
  sc->ebx = thr->ebx;
  sc->ecx = thr->ecx;
  sc->edx = thr->edx;
  sc->esi = thr->esi;
  sc->edi = thr->edi;
  sc->esp = sc->esp_at_signal = thr->esp;
  sc->ebp = thr->ebp;
  sc->eip = thr->eip;
  sc->eflags = thr->eflags;
  
  int_enabled = thr->int_enabled;
  priv_level = thr->priv_level;
  
  active_thread_num = thread;
  
  if (int_enabled) emu_int_enable();
}

void copy_registers(struct emu_thread *src, struct emu_thread *dst) {
  int *src2, *dst2;
  int i;
  
  src2 = (int*)src;
  dst2 = (int*)dst;
  
  for(i=0; i<10; i++) {
    dst2[i] = src2[i];
  }
  
}

void emu_bootstrap() {
  int init;
  
  init = open("init.img", O_RDONLY, 0);
  
  if (init == -1) {
    syserr("could not open initial task image init.img");
  }

  if (read(init, &mem_space[0x700000], 0x100000) == -1) {
    syserr("could not read initial task image init.img");
  }
  
}

void emu_int_enable() {
  struct itimerval emu_freq;
  struct itimerval old_freq;
  
  int_enabled = 1;
  
  emu_freq.it_interval.tv_sec = emu_freq.it_value.tv_sec = (long) 0;
  emu_freq.it_interval.tv_usec = emu_freq.it_value.tv_usec = (long) 100000;
  
  setitimer(ITIMER_VIRTUAL, &emu_freq, &old_freq);

}

void emu_int_disable() {
  struct itimerval emu_freq;
  struct itimerval old_freq;
  
  emu_freq.it_interval.tv_sec = emu_freq.it_value.tv_sec = (long) 0;
  emu_freq.it_interval.tv_usec = emu_freq.it_value.tv_usec = (long) 0;
  
  setitimer(ITIMER_VIRTUAL, &emu_freq, &old_freq);
  
  int_enabled = 0;
  
}

void emu_begin_simulation(int thread) { 
  run_thread(thread);
  copy_registers(&thread_state_table[active_thread_num=thread], &dispatch_thread);
  dispatch_from_scratch();
} 

int emu_int_state() { return int_enabled; }
char *emu_mem_base() { return mem_space; }

void emu_print_active_regs() {
  struct emu_thread t;
  struct sigcontext *x;
  
  x = (struct sigcontext*) (entry_esp+8);
  
  t.eax = x->eax;
  t.ebx = x->ebx;
  t.ecx = x->ecx;
  t.edx = x->edx;
  t.esi = x->esi;
  t.edi = x->edi;
  t.esp = x->esp;
  t.ebp = x->ebp;
  t.eflags = x->eflags;
  t.eip = x->eip;
  
  emu_print_regs(&t);
}

void emu_print_regs(struct emu_thread *thr) {
  fprintf(stdout, "eax = 0x%x\nebx = 0x%x\necx = 0x%x\nedx = 0x%x\nesi = 0x%x\nedi = 0x%x\nebp = 0x%x\nesp = 0x%x\neflags = 0x%x\neip = 0x%x\n", thr->eax, thr->ebx, thr->ecx, thr->edx, thr->esi, thr->edi, thr->ebp, thr->esp, thr->eflags, thr->eip);
  fflush(stdout);
}

void emu_output(char *x) {
  fprintf(stdout, x);
  fflush(stdout);
}

int emu_pid() { return getpid(); }
