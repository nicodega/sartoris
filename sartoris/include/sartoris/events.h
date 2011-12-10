
#ifndef EVENTS_H
#define EVENTS_H

struct evt_msg
{
    int evt;
    int id;
    int param;
    int padding;
}  __attribute__ ((__packed__));

#define SARTORIS_EVT_MSG   1

extern int evt_task;
extern int evt_thread;
extern int evt_interrupt;
extern struct port *evt_port;

void evt_raise(int id, int evt, int evt_param);

#endif
