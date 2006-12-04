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


#ifndef PMANTYPESH
#define PMANTYPESH

#ifndef NULL
#	define NULL 0
#endif

#define TRUE 1
#define FALSE 0

typedef unsigned char      BOOL;
typedef unsigned char      BYTE;
typedef unsigned int       UINT32;
typedef int	               INT32;
typedef unsigned long long UINT64;
typedef long long          INT64;
typedef unsigned short     UINT16;
typedef short              INT16;
typedef unsigned char      UINT8;
typedef void*              ADDR;

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#endif

