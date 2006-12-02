#include <sartoris/syscall.h>
#include <emu-services.h>

extern void init_syscalls(void);

char _msg0[] = "\n\nMAIN THREAD RUNNING\n\n";
char _msg1[] = "\n\nTHREAD ONE RUNNING\n\n";
char _msg2[] = "\n\nMAIN THREAD BACK IN CONTROL\n\n";
char _err[] = "\n\nERROR CREATING THREAD\n\n";

int thread1(void);

int main() {
  char *msg0, *msg2;
  
  struct thread thr;
  
  thr.task_num=INIT_TASK_NUM;
  thr.type = SRV_TASK | RUNNABLE;
  thr.irq = 0;
  thr.ep = (int) &thread1;
  thr.stack = 0xf0000-0x4;
  
  msg0 = (char*) ((int)(get_reloc()) + (int)_msg0) ;
  emu_scr_print(msg0);
  
  if(create_thread(0, &thr)==-1) {
    msg2 = (char*) ((int)(get_reloc()) + (int)_err) ;
    emu_scr_print(msg2);
  }
  
  run_thread(0);
  
  msg2 = (char*) ((int)(get_reloc()) + (int)_msg2) ;
  emu_scr_print(msg2);
  
  while(1);
  
}

int thread1(void) {
  char *msg1;
  struct thread thr;
  
  thr.task_num=INIT_TASK_NUM;
  thr.type = SRV_TASK | RUNNABLE;
  thr.irq = 0;
  thr.ep = (int) &thread1;
  thr.stack = 0xf0000-0x4;
  
  create_thread(1, &thr);
  
  msg1 = (char*) ((int)(get_reloc()) + (int)_msg1) ;
  emu_scr_print(msg1);
  
  run_thread(INIT_THREAD_NUM);
  
  for (;;);
}
