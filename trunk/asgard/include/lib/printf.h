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

#ifndef _PRINTF_H
#define _PRINTF_H

#include <lib/const.h>
#include <lib/malloc.h>
#include <lib/sprintf.h>
#include <lib/structures/string.h>
#include <lib/iolib.h>

#ifdef __cplusplus
extern "C" {
#endif

int printf(char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
