/*
*	Process and Memory Manager Service
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

#ifndef _EXCEPT_H_
#define _EXCEPT_H_

#define DIV_ZERO        0
#define DEBUG           1
#define BREAKPOINT      3
#define OVERFLOW        4
#define BOUND           5
#define INV_OPC         6
#define DEV_NOT_AV      7
#define STACK_FAULT     12
#define GEN_PROT        13
#define PAGE_FAULT      14
#define FP_ERROR        16
#define ALIG_CHK        17
#define SIMD_FAULT      19


#define SART_EVENTS_INT     63

void fatal_exception(UINT16 task_id, INT32 ret_value);
void exception_signal(UINT16 task_id, UINT16 thread_id, UINT16 exception, ADDR last_exception_addr);

struct exception_management
{
    unsigned short exceptions_port;
    unsigned short ecount;
    ADDR last_exception_addr;
};

#endif
