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

#ifndef STDLIBSIMH
#define STDLIBSIMH

#ifdef __cplusplus
extern "C" {
#endif

typedef int size_t;
typedef unsigned int uint_t;

void *memcpy( void *to, const void *from, size_t count );
void *memmove (unsigned char *dest, unsigned char *src, size_t n);
void *memset(void *s, int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif
