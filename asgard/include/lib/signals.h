/*
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*     modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#include <services/pmanager/signals.h>

#ifndef SIGNALSH
#define SIGNALSH

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

struct signal_response
{
    int thr;     // waiting thread
    int task;    // task from whom the message must be received
    unsigned char event_type;
    unsigned char id;
    unsigned int param;
    unsigned int res;
    unsigned short ret;
    int received;    // initialized to zero, if message arrives, it'll be set to 1
    int exec;        // signal is being executed
} PACKED_ATT;

typedef struct 
{
    void *ss_sp;
    unsigned int ss_size;
    int ss_flags;
} stack_t;

#define SIGNALS_THRINFO_STATE_PENDING  0
#define SIGNALS_THRINFO_STATE_ACK      1
#define SIGNALS_THRINFO_STATE_OK       2
#define SIGNALS_THRINFO_STATE_ERR      3

typedef struct _signals_thr_info
{
    unsigned short thr_id;
    unsigned short state;
    stack_t stack_info;
    struct _signals_thr_info *next;
    struct _signals_thr_info *prev;
} signals_thr_info;

typedef struct signal_response** SIGNALHANDLER;

#define SIGNALS_PORT    5

/* Inifinite value for signal timeout */
#define SIGNAL_TIMEOUT_INFINITE   PMAN_SIGNAL_TIMEOUT_INFINITE

/* Define to ignore a param of an event (i.e. generate the signal ignoring the param) */
#define SIGNAL_PARAM_IGNORE       PMAN_SIGNAL_PARAM_IGNORE

void sleep(unsigned int milliseconds);
int wait_signal(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned int param, unsigned int *res);
SIGNALHANDLER wait_signal_async(unsigned short event_type, unsigned short task, unsigned int timeout, unsigned int param);
int wait_sigh(SIGNALHANDLER sigh);
int check_signal(SIGNALHANDLER sigh, unsigned int *res);
void discard_signal(SIGNALHANDLER sigh);
int signal_id(SIGNALHANDLER sigh);
void send_event(unsigned short task, unsigned short event_type, unsigned int param, unsigned int res);

void call_with_ss_swap(void *addr, void *ss);

typedef void (*sighandler_t)(int signum);

sighandler_t signal(int id, sighandler_t handler);
int raise(int id);
int sigaltstack(stack_t *ss, stack_t *oss);

#define SIG_IGN (sighandler_t)0
#define SIG_DFL (sighandler_t)1
#define SIG_ERR (sighandler_t)-1

#ifdef __cplusplus
}
#endif

#endif
