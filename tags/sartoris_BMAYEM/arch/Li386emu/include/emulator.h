#ifndef _EMU_H_
#define _EMU_H_

#define EMU_SYSCALL_SIGNAL SIGUSR1
#define EMU_IO_SIGNAL SIGUSR2

/* syscalls */

#define CREATE_TASK 0
#define DESTROY_TASK 1
#define CREATE_THREAD 2
#define DESTROY_THREAD 3
#define RUN_THREAD 4
#define SEND_MSG 5
#define GET_MSG 6
#define GET_MSG_COUNT 7
#define SHARE_MEM 8
#define CLAIM_MEM 9
#define READ_MEM 10
#define WRITE_MEM 11
#define PASS_MEM 12
#define SET_THREAD_RUN_PERMS 13
#define SET_THREAD_RUN_MODE 14
#define SET_PORT_PERMS 15
#define SET_PORT_MODE 16
#define CLOSE_PORT 17
#define OPEN_PORT 18

struct emu_thread {
  
  /* user registers */
  int eax;
  int ebx;
  int ecx;
  int edx;
  int esi;
  int edi;
  int ebp;
  int esp;
  int eip;
  int eflags;
  
  /* system registers */
  int int_enabled;
  int priv_level;

};


void emu_startup(void); /* maybe to do a "reboot" someday */
void emu_set_thread_state_base(struct emu_thread *t);
int emu_dispatch(int thread);
void emu_int_enable(void);
void emu_int_disable(void);
int emu_int_state(void);
char *emu_mem_base(void);
void emu_print_regs(struct emu_thread *thr);
void emu_print_active_regs(void);

void emu_begin_simulation(int thread);

void emu_output(char *x);

int emu_pid(void);

#endif
