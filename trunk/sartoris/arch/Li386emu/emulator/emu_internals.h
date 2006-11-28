#ifndef _EMU_INT_H_
#define _EMU_INT_H_

#include "sartoris/kernel.h"
#include "emulator.h"

/* =============== emulator defines ================= */

#define EMU_MEM_SIZE 0x800000
#define EMU_MAX_NESTED 32

#define EMU_NUM_PORTS 1024

#define EMU_CLOCK_SIGNAL SIGVTALRM

/* actions that trigger the trampoline */

#define ACT_CLOCK 0
#define ACT_SYSCALL 1
#define ACT_IO 2

/* =================================================== */

extern void sigtimer_handler(int);
extern void syscall_handler(int);

extern int entry_esp;
extern int emu_action;

extern struct emu_thread dispatch_thread;
extern void dispatch_from_scratch(void);

void emu_trampoline(void);
void emu_bootstrap(void);

void copy_registers(struct emu_thread *src, struct emu_thread *dst);

void syserr(char *msg);

#endif
