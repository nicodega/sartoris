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
#define PCI_PROGIF   	0x09
#define PCI_SCLASS		0x0a
#define PCI_CLASS		0x0b
#define PCI_CACHELINE	0x0c
#define PCI_LATENCY		0x0d
#define PCI_HEADER		0x0e
#define PCI_SELFTEST	0x0f
#define PCI_BASEREG0	0x10
#define PCI_BASEREG1	0x14

// only for devices
#define PCI_BASEREG2	 0x18
#define PCI_BASEREG3	 0x1c
#define PCI_BASEREG4	 0x20
#define PCI_BASEREG5	 0x24
#define PCI_CARDBUSCIS	 0x28
#define PCI_SUBSYSVENDOR 0x2c
#define PCI_SUBSYSID     0x2e
#define PCI_EXPASIONROM	 0x30
#define PCI_IRQLINE		 0x3c
#define PCI_INTERRUPTPIN 0x3d
#define PCI_MINGRANT     0x3e
#define PCI_MAXLATENCY	 0x3f

// only for bridges
#define PCI_PBN        0x18
#define PCI_SBN        0x19
#define PCI_SUBBN      0x1a
#define PCI_SLT        0x1b
#define PCI_IOBASE     0x1c
#define PCI_IOLIMIT    0x1d
#define PCI_SECSTATUS  0x1e
#define PCI_MEMBASE    0x20
#define PCI_MEMLIMIT   0x22
#define PCI_PREFMEMBASE 0x24
#define PCI_PREFMEMLIM  0x26
#define PCI_PREFBASEU32 0x28
#define PCI_PREFLIMU32  0x2C
#define PCI_IOBASEU32   0x30
#define PCI_IOLIMU32    0x32
#define PCI_CAPPTR      0x34
#define PCI_EXPROMBASE  0x38
#define PCI_INTLINE     0x3C
#define PCI_INTPIN      0x3D
#define PCI_BRIDGECTRL  0x3E

#define PCI_HEADER_MF 0x80   // device has multiple functions
#define PCI_HEADER_TYPE(x) (x & ~0x80)   // device has multiple functions

/* These are for the header field */
#define PCI_TYPE_DEVICE    0
#define PCI_TYPE_BRIDGE    1
#define PCI_TYPE_CBBRIDGE  2

struct pci_dev
{
	unsigned int id;
    unsigned int dev;
	unsigned int function;

	BYTE header;
    WORD deviceid;
	WORD vendorid;
	BYTE class;
    BYTE sclass;

	WORD status;
	WORD command;
	BYTE progif;
	BYTE revision;
	BYTE self_test;
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

	BYTE max_latency;
	BYTE min_grant;
	BYTE interupt_pin;
	BYTE irq_line;

    struct pci_dev *next;

    struct pci_dev *gnext;
    unsigned int eid;
};

struct pci_bridge
{	
    unsigned int id;
    unsigned int dev;
	unsigned int function;

	BYTE header;
    WORD deviceid;
	WORD vendorid;
	BYTE class;
    BYTE sclass;
	
    WORD status;
	WORD command;
	BYTE progif;
	BYTE revision;
	BYTE self_test;
	BYTE latency;
	BYTE cache_line_size;

    DWORD baseaddrr0;
	DWORD baseaddrr1;

    BYTE sec_latency_timer;
    BYTE subordinate_bus;
    BYTE secondary_bus;
    BYTE primary_bus;
    WORD secondary_status;
    BYTE io_imit;
    BYTE io_base;
    WORD memory_limit;
    WORD memory_base;
    WORD prefetch_mem_limit;
    WORD prefetch_mem_base;
    DWORD prefetch_base_upper32;
    DWORD prefetch_limit_upper32;
    WORD io_limit_upper16;
    WORD io_base_upper16;
    BYTE capability_ptr;
    DWORD expansion_rom_base;
    WORD bridge_control;
	BYTE interupt_pin;
	BYTE irq_line;

    struct pci_bridge *gnext;
    unsigned int eid;
};

#define PCI_DEVID_BUS(x) (x >> 16)
#define PCI_DEVID_EID(x) (x & 0x0000FFFF)
#define PCI_DEVID_IS_BRIDGE(x) ((x & 0x0000FFFF) > 255)

struct pci_bus
{
    int id;
    struct pci_dev *first_dev;
    struct pci_bridge *bridge_up;  // if not null this bus comes from it's parent though this bridge
    struct pci_bus *first_child;   // child buses
    struct pci_bus *parent;        // the parent bus
    struct pci_bus *next;
};

extern struct pci_bus bus0;
extern int busesc;
extern struct pci_dev *devices;
extern struct pci_bridge *bridges;

/* Functions */
BYTE read_pci_cnfb(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
WORD read_pci_cnfw(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
DWORD read_pci_cnfd(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
void write_pci_cnfw(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg, WORD value);
void write_pci_cnfd(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg, DWORD value);
BYTE read_pci_cnfb(unsigned int bus, unsigned int dev, unsigned int func, unsigned int reg);
void outb(WORD port, BYTE value);
BYTE inb(WORD port);
void outw(WORD port, WORD value);
WORD inw(WORD port);
void outd(WORD port, DWORD value);
DWORD ind(WORD port);

void enum_pci();
struct pci_bridge *get_bridge(int bus_id, int bridge_id);
struct pci_dev *get_device(int bus_id, int device_id);

void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
void process_pci_cmd(struct pci_cmd *pcmd, int task);
struct pci_res *build_response_msg(int ret);

void get_devices(struct pci_getdevices *pcmd, struct pci_res *res);
void pci_find(struct pci_finddev *pcmd, struct pci_res *res);
void get_config(struct pci_getcfg *pcmd, struct pci_res *res);
void set_config(struct pci_setcfg *pcmd, struct pci_res *res);

struct pci_dev *get_device(int bus_id, int device_id);
void read_device(struct pci_dev *pcidev);
void read_bridge(struct pci_bridge *bridge);

#endif

