/*
*	Implementation of the SHA-1 algorithm for byte strings hashing.
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

#ifndef SHA1H
#define SHA1H

#include <lib/malloc.h>

#define LITTLE_ENDIAN 

// padding expects size in bytes... //
unsigned int padding(unsigned char *str,unsigned int length, unsigned int **pstr);
void compute_hash(unsigned int *padded_str, int length, unsigned int H[5]);


#endif
