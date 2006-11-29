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

#define DIV_ZERO 0
#define OVERFLOW 4
#define BOUND    5
#define INV_OPC  6
#define DEV_NOT_AV 7
#define STACK_FAULT 12
#define GEN_PROT 13
#define PAGE_FAULT 14
#define FP_ERROR 16
#define ALIG_CHK 17
#define SIMD_FAULT 19

/* Return values sent to task creator */
#define DIV_ZERO_RVAL		-2
#define OVERFLOW_RVAL		-3
#define BOUND_RVAL			-4
#define INV_OPC_RVAL		-5
#define DEV_NOT_AV_RVAL		-6
#define STACK_FAULT_RVAL	-7
#define GEN_PROT_RVAL		-8
#define PAGE_FAULT_RVAL		-9
#define FP_ERROR_RVAL		-10
#define ALIG_CHK_RVAL		-11
#define SIMD_FAULT_RVAL		-12

#endif
