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

#include <lib/critical_section.h>
#include <lib/structures/stdlibsim.h>

#ifndef MALLOCH
#define MALLOCH

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nelem, size_t elsize);
void free(void *ptr);
void init_mem(void *buffer, unsigned int size);
void close_malloc_mutex();
unsigned int free_mem();

extern void _exit(int) __attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
