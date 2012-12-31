
#ifndef EVENTS_H
#define EVENTS_H

extern int evt_task;
extern int evt_thread;
extern int evt_interrupt;
extern struct port *evt_port;

void ARCH_FUNC_ATT3 evt_raise(int id, int evt, int evt_param);

#endif
