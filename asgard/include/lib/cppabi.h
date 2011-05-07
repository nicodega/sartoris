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

#ifndef CPPABIH
#define CPPABIH

#define _ATEXIT_MAXFUNCS 128

#ifdef __cplusplus
extern "C" {
#endif
     
struct atexit_func
{
	void (*dtor)(void *);
	void *ptr;
	void *dso_handle;
};
 
int __cxa_atexit(void (*f)(void *), void *ptr, void *dso);
void __cxa_finalize(void *f);
    
#ifdef __cplusplus
}
#endif

#endif
