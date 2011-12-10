
#ifndef EVENTS_H
#define EVENTS_H

extern int evt_task;
extern int evt_thread;
extern int evt_interrupt;
extern struct port *evt_port;

void evt_raise(int id, int evt, int evt_param);

#endif
