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

#include <lib/io.h>

/* C interface for i386 in and out assembler commands. */

/* Read a DWORD from an IO port */
DWORD ind(WORD port)
{
	DWORD res;
	__asm__ __volatile__ ("xorl %%eax,%%eax;inl %%dx,%%eax; movl %%eax, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a DWORD onto a port */
void outd(WORD port, DWORD value)
{
	__asm__ __volatile__("outl %%eax, %%dx" : : "d" (port), "a" (value));
}

/* Read a BYTE from an IO port */
WORD inw(WORD port)
{
	WORD res;
	__asm__ __volatile__("xorl %%eax,%%eax;inw %%dx,%%ax; movw %%ax, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a BYTE onto a port */
void outw(WORD port, WORD value)
{
	__asm__ __volatile__ ("outw %%ax, %%dx" : : "d" (port), "a" (value));
}

/* Read a BYTE from an IO port */
BYTE inb(WORD port)
{
	BYTE res;
	__asm__ __volatile__ ("xorl %%eax,%%eax;inb %%dx,%%al;movb %%al, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a BYTE onto a port */
void outb(WORD port, BYTE value)
{
	__asm__ __volatile__ ("outb %%al, %%dx" : : "d" (port), "a" (value) );
}

/* Rep version of the functions */

void rind(WORD port, DWORD buffer, DWORD count)
{
	__asm__ __volatile__ ("cld;rep;insl;" : : "d" (port),"D" (buffer),"c" (count) );
}

void routd(WORD port, DWORD buffer, DWORD count)
{
	__asm__ __volatile__ ("cld;rep;outsl;" : : "d" (port),"S" (buffer),"c" (count) );
}

void rinw(WORD port, DWORD buffer, WORD count)
{
	__asm__ __volatile__ ("cld;rep;insw;" : : "d" (port),"D" (buffer),"c" (count) );
}
void routw(WORD port, DWORD buffer, WORD count)
{
	__asm__ __volatile__ ("cld;rep;outsw;" : : "d" (port),"S" (buffer),"c" (count) );
}

void rinb(WORD port, DWORD buffer, WORD count)
{
	__asm__ __volatile__ ("cld;rep;insb;" : : "d" (port),"D" (buffer),"c" (count) );
}
void routb(WORD port, DWORD buffer, WORD count)
{
	__asm__  __volatile__ ("cld;rep;outsb;" : : "d" (port),"S" (buffer),"c" (count) );
}



