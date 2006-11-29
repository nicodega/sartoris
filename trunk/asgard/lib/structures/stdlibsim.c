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

#include <lib/structures/stdlibsim.h>

// NOTE: This functions should be optimized in a future

void *memcpy( void *to, const void *from, size_t count )
{

	int i = 0;
	while(i < count){
		((unsigned char *)to)[i] = ((unsigned char *)from)[i];
		i++;
	}

	return to;
}

void *memmove (unsigned char *dest, unsigned char *src, size_t n)
{
  if ((src <= dest && src + n <= dest) || src >= dest)
    while (n-- > 0)
      *dest++ = *src++;
  else
    {
      dest += n;
      src += n;
      while (n-- > 0)
	*--dest = *--src;
    }

  return dest;
}

void *memset(void *s, int c, size_t n)
{
	unsigned char *src = (unsigned char*)s;
	while(n > 0)
	{
		*src++ = (unsigned char)c;
		n--;
	}

	return s;
}
