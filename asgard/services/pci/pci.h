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

#include <services/stds/stdservice.h>
#include <services/pci/pci.h>
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <lib/const.h>

#ifndef PCIH
#define PCIH

typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

#define GET_SUBCLASS(x) ((x & 0xFF00) >> 8)
#define GET_PROGIF(x) (x & 0xFF)
#define GET_CLASS(x) ((x & 0xFF0000) >> 16)

#define	PCI_CONFADR	0xCF8
#define	PCI_CONFDATA 0xCFC

/* Registers within config space for a bus/device/func */
#define PCI_VENDORID	0x00
#define PCI_DEVICEID	0x02
#define PCI_COMMAND		0x04
#define PCI_STATUS		0x06
#define PCI_REVISION	0x08
#define PCI_CLASS		0x09
#define PCI_CACHELINE	0x0c
#define PCI_LATENCY		0x0d
#define PCI_HEADER		0x0e
#define PCI_SELFTEST	0x0f
#define PCI_BASEREG0	0x10
#define PCI_BASEREG1	0x14
#define PCI_BASEREG2	0x18
#define PCI_BASEREG3	0x1c
#define PCI_BASEREG4	0x20
#define PCI_BASEREG5	0x24
#define PCI_CARDBUSCIS	0x28
#define PCI_SUBSYSVENDOR	0x2c
#define PCI_SUBSYSID	0x2e
#define PCI_EXPASIONROM	0x30
#define PCI_IRQLINE		0x3c
#define PCI_INTERRUPTPIN	0x3d
#define PCI_MINGRANT	0x3e
#define PCI_MAXLATENCY	0x3f


struct pci_dev
{
	unsigned int id;
	unsigned int bus;
	unsigned int dev;
	unsigned int func;

	WORD deviceid;
	WORD vendorid;
	WORD status;
	WORD command;
	WORD class_sclass;
	BYTE progif;
	BYTE revision;
	BYTE self_test;
	BYTE header;
	BYTE latency;
	BYTE cache_line_size;

	DWORD baseaddrr0;
	DWORD baseaddrr1;
	DWORD baseaddrr2;
	DWORD baseaddrr3;
	DWORD baseaddrr4;
	DWORD baseaddrr5;

	DWORD cardbus_CIS;

	WORD subsystemid;
	WORD subsystemvendorid;

	DWORD expromaddr;

	DWORD res0;
	DWORD res1;

	BYTE max_latency;
	BYTE min_grant;
	BYTE interupt_Pin;
	BYTE irq_Line;
};

struct pci_bus
{
	
};

/* Functions */
BYTE read_pci_cnfb(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
WORD read_pci_cnfw(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
DWORD read_pci_cnfd(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
void outb(WORD port, BYTE value);
BYTE inb(WORD port);
void outw(WORD port, WORD value);
WORD inw(WORD port);
void outd(WORD port, DWORD value);
DWORD ind(WORD port);

void enum_pci();

void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
void process_pci_cmd(struct pci_cmd *pcmd, int task);
struct pci_res *build_response_msg(int ret);

struct pci_res *set_config(struct pci_setcfg *pcmd);
struct pci_res *get_config(struct pci_getcfg *pcmd);
struct pci_res *get_devices(struct pci_getdevices *pcmd);
struct pci_res *get_mapped_regions(struct pci_getmregions *pcmd);
struct pci_dev *get_dev(BYTE class, WORD sclass, WORD vendorid, BYTE function);
struct mem_region *get_mregion(struct pci_dev *pcidev, int addr, int *cont);


#endif

