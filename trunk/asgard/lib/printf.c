
#include <lib/printf.h>

/* 	This is a simple printf implementation. It should be useful as 
* 	a starting point for a future serious one. I have not implemented snprintf
* 	or vsnprintf, and those are a must. Bear in mind that passing a long 
* 	expression to printf will certainly crash, as I used a buffer in the stack.
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

// this version uses stdout
int printf(char *format, ...) 
{
	char buf[512];
	
	/* call vsprintf to construct string */
	vsprintf(buf, format, (int*) (&format + 1));

	if(fputs(buf, &stdout) < 0)
	{
		return 1;
	}
	
	return 0;	
	
}
