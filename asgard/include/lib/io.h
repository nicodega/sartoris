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

#ifndef _IOH_
#define _IOH_

typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

DWORD ind(WORD port);
void outd(WORD port, DWORD value);

WORD inw(WORD port);
void outw(WORD port, WORD value);

BYTE inb(WORD port);
void outb(WORD port, BYTE value);

/* Rep version */
void rind(WORD port, DWORD buffer, DWORD count);
void routd(WORD port, DWORD buffer, DWORD count);

void rinw(WORD port, DWORD buffer, WORD count);
void routw(WORD port, DWORD buffer, WORD count);

void rinb(WORD port, DWORD buffer, WORD count);
void routb(WORD port, DWORD buffer, WORD count);

#endif



