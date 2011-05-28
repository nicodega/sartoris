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

#include "pci.h"

/* Read a DWORD from an IO port */
DWORD ind(WORD port)
{
	DWORD res;
	__asm__("inl %%dx,%%eax; movl %%eax, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a DWORD onto a port */
void outd(WORD port, DWORD value)
{
	__asm__("outl %%eax, %%dx" : : "d" (port), "a" (value));
}

/* Read a BYTE from an IO port */
WORD inw(WORD port)
{
	WORD res;
	__asm__("inw %%dx,%%ax; movw %%ax, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a BYTE onto a port */
void outw(WORD port, WORD value)
{
	__asm__("outw %%ax, %%dx" : : "d" (port), "a" (value));
}

/* Read a BYTE from an IO port */
BYTE inb(WORD port)
{
	BYTE res;
	__asm__("inb %%dx,%%al;movb %%al, %0" : "=r" (res) : "d" (port) : "eax");
	return res;
}

/* Write a BYTE onto a port */
void outb(WORD port, BYTE value)
{
	__asm__("outb %%al, %%dx" : : "d" (port), "a" (value) );
}

DWORD read_pci_cnfd(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address);            // set up addressing to config data
    DWORD value = ind(PCI_CONFDATA) ;  // get requested DWORD of config data
    return value;
}

WORD read_pci_cnfw(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address) ;         // set up addressing to config data
    WORD value = inw(PCI_CONFDATA) ;    // get requested DWORD of config data
    return value;
}

BYTE read_pci_cnfb(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address);			// set up addressing to config data
    BYTE value = inb(PCI_CONFDATA);		// get requested DWORD of config data
    return value;
}

void write_pci_cnfd(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg, DWORD value)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address) ;         // set up addressing to config data
    outd(PCI_CONFDATA, value) ;         // set requested DWORD of config data
}

void write_pci_cnfw(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg, WORD value)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address) ;         // set up addressing to config data
    outw(PCI_CONFDATA, value) ;         // set requested DWORD of config data
}

void write_pci_cnfb(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg, BYTE value)
{
	/* 0x80000000 because the use bit is on */
	DWORD address = 0x80000000 | (((DWORD)(bus & 0xFF)) << 16) |
                      (((dev) & 0x1F) << 11) |
                      (((func) & 0x07) << 8) | (reg & 0xFC) ;
	outd(PCI_CONFADR,address);			// set up addressing to config data
    outb(PCI_CONFDATA, value) ;         // set requested DWORD of config data
}

