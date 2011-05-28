/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

/* 
Signaling on pmanager will allow a given thread to sleep 
until a signal arrives. (this will be extended when stdsignals is
implemented)

NOTE: A given thread won't be allowed to wait for the same signal, coming
from the same task twice. When a signal times out, or an event arrives 
the signal queue will be checked, and if a new WAIT_FOR_SIGNAL event is 
present, with same task, event_type and thread, that signal
will override the current signal.
*/

#ifndef PMANDEBUGH
#define PMANDEBUGH

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#define DEBUG_THR_CREATED       1
#define DEBUG_THR_DESTROYED     2
#define DEBUG_EXCEPTION         3
#define DEBUG_TASK_FINISHED     4

struct dbg_message
{
    int command;
    short task;         // id of the task
    short thread;       // id of the thread
    int status;         // status will only be useful on DEBUG_THR_CREATED
    int padding;
};

struct dbg_exception_message
{
    int command;
    short task;         // id of the task
    short thread;       // id of the thread
    int exception;      // status will only be useful on DEBUG_THR_CREATED
    int error_code;     // the error code of the exception
};

#define DEBUG_STATUS_OK         0
#define DEBUG_STATUS_DBG_FAILED 1

#define DEBUG_EXE_DIV_ZERO        0
#define DEBUG_EXE_DEBUG           1
#define DEBUG_EXE_BREAKPOINT      3
#define DEBUG_EXE_OVERFLOW        4
#define DEBUG_EXE_BOUND           5
#define DEBUG_EXE_INV_OPC         6
#define DEBUG_EXE_DEV_NOT_AV      7
#define DEBUG_EXE_STACK_FAULT     12
#define DEBUG_EXE_GEN_PROT        13
#define DEBUG_EXE_PAGE_FAULT      14
#define DEBUG_EXE_FP_ERROR        16
#define DEBUG_EXE_ALIG_CHK        17
#define DEBUG_EXE_SIMD_FAULT      19

#define DEBUG_MAP_EXCEPTION(x) x

#endif
