
/*
This include file contains sartoris extended errors.
To get the last error you can invoke last_error syscall.
*/

#ifndef _ERRORH
#define _ERRORH

#include "sartoris/kernel-data.h"

#define SERR_OK                       0
#define SERR_INVALID_PTR              1
#define SERR_INVALID_ID               2
#define SERR_INVALID_TSK              3
#define SERR_INVALID_THR              4
#define SERR_NO_PERMISSION            5
#define SERR_NO_MEM                   6
#define SERR_ERROR                    7
#define SERR_ALLOCATING               8
#define SERR_MISSING_PERMS_BITMAP     9
#define SERR_SAME_THREAD              10
#define SERR_SAME_TASK                11
#define SERR_INVALID_SIZE             12
#define SERR_INVALID_MODE             13
#define SERR_INVALID_BASE             14
#define SERR_INVALID_PRIV             15
#define SERR_INVALID_PORT             16
#define SERR_PORT_DISABLED            17
#define SERR_PORT_FULL                18
#define SERR_NO_MSG                   19
#define SERR_NOT_ALLOCATING           20
#define SERR_NO_PAGING                21
#define SERR_NO_PAGEFAULT             22
#define SERR_TOO_MANY_SMOS            23
#define SERR_INVALID_SMO              24
#define SERR_NOT_SMO_TARGET           25
#define SERR_SMO_NOT_OWNED            26
#define SERR_NO_IDS                   27
#define SERR_INVALID_INTERRUPT        28
#define SERR_INTERRUPT_HANDLED        29
#define SERR_NO_INTERRUPT             30
#define SERR_INTERRUPT_NOT_ACTIVE     31
#define SERR_INTERRUPT_NOT_NESTING    32
#define SERR_INTERRUPTS_MAXED         33
#define SERR_NOT_TRACING              34
#define SERR_ALREADY_TRACING          35
#define SERR_INVALID_REG              36

#ifdef __KERNEL__
static inline void set_error(short error)
{
    GET_PTR(curr_thread,thr)->last_error = error;
}

int last_error();
#endif

#endif
