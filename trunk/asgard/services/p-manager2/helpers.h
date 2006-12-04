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


#include "types.h"

#ifndef HELPERSH
#define HELPERSH

#define MIN(a,b) ((a < b)? a : b)
#define MAX(a,b) ((a < b)? b : a)

void map_pages(INT32 task, UINT32 linear_start, UINT32 physical_start, UINT32 count, INT32 flags, INT32 level);

#define STOP for(;;);

#endif

