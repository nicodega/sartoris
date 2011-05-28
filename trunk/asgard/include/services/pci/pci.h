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

#ifndef _PCIH_
#define _PCIH_

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif


#define PCI_UIID	0x00000009
#define PCI_VER		0x1

/* Find a device */
#define PCI_FIND                1
/* Get configuration for an specific device */
#define PCI_GETCONFIG           2
/* Set configuration for a device */
#define PCI_CONFIGURE           3
/* Set configuration for a device */
#define PCI_GETDEVICES          4

/* Values for the ret field on pci_res */
#define PCIERR_OK					0
#define PCIERR_DEVICE_NOT_FOUND		1
#define PCIERR_INVALID_CONTINUATION	2
#define PCIERR_INVALID_COMMAND		4
#define PCIERR_SMO_TOO_SMALL		5
#define PCIERR_NOTIMPLEMENTED		6
#define PCIERR_UNKNOWN				7
#define PCIERR_SMO_WRITE_ERR		8
#define PCIERR_ENUM_FINISHED		9
#define PCIERR_INVALID_DEVID        10
#define PCIERR_NOT_FOUND            11
#define PCIERR_SMO_READ_ERR         12

/* Generic command format */
struct pci_cmd
{
	short command;
	short thr_id;
	int specific0;
	int specific1;
	short specific2;
	char specific3;
	char ret_port;
} PACKED_ATT;

/* Generic response format */
struct pci_res
{
	short command;
	short thr_id;
	int specific0;
	int specific1;
	short ret;
	short ret_port;
} PACKED_ATT;


struct pci_finddev_res
{
	short command;
	short thr_id;
	unsigned int dev_id;
	char function;
    char progif;
    char padding[2];
	short ret;
	short ret_port;
} PACKED_ATT;

/* This function will return the ID of a device 
with the specified class/subclass/vendor/[function]/[progif] */ 
struct pci_finddev
{
	short command;
	short thr_id;
	char class;
    char sclass;
    char function;
    short vendorid;
    char progif;
    char flags;
    char padding[4];
	char ret_port;
} PACKED_ATT;

#define FIND_FLAG_NONE     0
#define FIND_FLAG_PROGIF   1
#define FIND_FLAG_FUNCTION 2

/* PCI_GETCONFIG */
struct pci_getcfg
{
	short command;
	short thr_id;	
	unsigned int dev_id;             // id returned by pci_finddev or the one on pci_dev_info
	char padding[3];
	short cfg_smo; 
	char ret_port;
} PACKED_ATT;

/* PCI_SETCONFIG */
/*
cfg_smo MUST be READ/WRITE, because once registers have been set
thei'll be written with the real contents of them.
*/
struct pci_setcfg
{
	short command;
	short thr_id;
	unsigned int dev_id;             // id returned by pci_finddev or the one on pci_dev_info
	short cfg_smo; 
    int flags;
    char padding;
	char ret_port;
} PACKED_ATT;

#define PCI_SETCFG_ALL          0xFFFFFFFF
#define PCI_SETCFG_STATUS       0x1
#define PCI_SETCFG_COMMAND      0x2
#define PCI_SETCFG_BIST         0x4
#define PCI_SETCFG_LATENCY      0x8
#define PCI_SETCFG_CACHELSIZE   0x10
#define PCI_SETCFG_BASEADDR0    0x20
#define PCI_SETCFG_BASEADDR1    0x40
#define PCI_SETCFG_BASEADDR2    0x80
#define PCI_SETCFG_BASEADDR3    0x100
#define PCI_SETCFG_BASEADDR4    0x200
#define PCI_SETCFG_BASEADDR5    0x400
#define PCI_SETCFG_EXP_ROM_ADDR  0x800
#define PCI_SETCFG_INT_PIN       0x1000
#define PCI_SETCFG_INT_LINE      0x2000
#define PCI_SETCFG_CIS_PTR       0x4000

#define PCI_SETCFG_SEC_LATENCY_TMR  0x80
#define PCI_SETCFG_SUBORD_BUS       0x100
#define PCI_SETCFG_SECONDARY_BUS    0x200
#define PCI_SETCFG_PRIMARY_BUS      0x400
#define PCI_SETCFG_SECONDARY_STATUS 0x4000
#define PCI_SETCFG_IO_LIMIT         0x8000
#define PCI_SETCFG_IO_BASE          0x10000
#define PCI_SETCFG_MEM_LIMIT        0x20000
#define PCI_SETCFG_MEM_BASE         0x40000
#define PCI_SETCFG_PREFETCHB_UP32   0x80000
#define PCI_SETCFG_PREFETCHL_UP32   0x100000
#define PCI_SETCFG_IO_LIMIT_UP16    0x200000
#define PCI_SETCFG_IO_BASE_UP16     0x400000
#define PCI_SETCFG_BRIDGE_CTRL      0x800000
#define PCI_SETCFG_PREF_MEM_LIMIT   0x1000000
#define PCI_SETCFG_PREF_MEM_BASE    0x2000000

/* PCI_GETDEVICES */
/* This message will return a continuation value on specific0.
The first message must be issued with continuation set to 0.
of it's ret msg. It will also return written unsigned chars on specific1. */
struct pci_getdevices
{
	short command;
	short thr_id;
	int buffer_smo;	
	int continuation;
	short specific2;
	char specific3;
	char ret_port;
} PACKED_ATT;

/* Structure copied onto pci_getdevices buffer. */
struct pci_dev_info
{
	char class;			/* class */
	short sclass;		/* sub class */
	short vendorid;		/* vendor id */
	char function;		/* 1...8 */
	char progif;
    int dev_id;
    int type;
} PACKED_ATT;

#define PCI_TYPE_DEVICE   0
#define PCI_TYPE_BRIDGE   1

/* Structure used for get/set config commands */
struct pci_cfg
{
	unsigned short status;
	unsigned short command;
	unsigned char progif;
	unsigned char revision;
	unsigned char self_test;
	unsigned char latency;
	unsigned char cache_line_size;

	unsigned int baseaddrr0;
	unsigned int baseaddrr1;
	unsigned int baseaddrr2;
	unsigned int baseaddrr3;
	unsigned int baseaddrr4;
	unsigned int baseaddrr5;

	unsigned int cardbus_CIS;

	unsigned short subsystemid;
	unsigned short subsystemvendorid;

	unsigned int expromaddr;
    
	unsigned char max_latency;
	unsigned char min_grant;
	unsigned char interupt_pin;
	unsigned char irq_line;
} PACKED_ATT;

struct pci_bridge_cfg
{
	unsigned short status;
	unsigned short command;
	unsigned char progif;
	unsigned char revision;
	unsigned char self_test;
	unsigned char latency;
	unsigned char cache_line_size;

	unsigned int baseaddrr0;
	unsigned int baseaddrr1;

    unsigned char sec_latency_timer;
    unsigned char subordinate_bus;
    unsigned char secondary_bus;
    unsigned char primary_bus;
    unsigned short secondary_status;
    unsigned char io_imit;
    unsigned char io_base;
    unsigned short memory_limit;
    unsigned short memory_base;
    unsigned short prefetch_mem_limit;
    unsigned short prefetch_mem_base;
    unsigned int prefetch_base_upper32;
    unsigned int prefetch_limit_upper32;
    unsigned short io_limit_upper16;
    unsigned short io_base_upper16;
    unsigned char capability_ptr;
    unsigned int expansion_rom_base;
    unsigned short bridge_control;
	unsigned char interupt_pin;
	unsigned char irq_line;
} PACKED_ATT;


/* These are for "self_test" field */
#define PCI_BIST_CAPABLE   0x80
#define PCI_BIST_STAR      0x40
#define PCI_BIST_COMP_CODE (x) (x & 0xF)    // BIST completion code

/* These are for the command field */
#define PCI_CMD_IOSPACE         0x01
#define PCI_CMD_MEMSPACE        0x02
#define PCI_CMD_BUSMASTER       0x04
#define PCI_CMD_MEMWANDINV_ENA  0x08
#define PCI_CMD_VGAPALETTESNOOP 0x10
#define PCI_CMD_PARITYERRRES    0x20
#define PCI_CMD_SEER_ENA        0x80
#define PCI_CMD_FB2BENA         0x100
#define PCI_CMD_INTDIS          0x200

/* These are for status registers */
#define PCI_STATUS_RES0         0x07
#define PCI_STATUS_INTSTATUS    0x08
#define PCI_STATUS_CAPLIST      0x10
#define PCI_STATUS_66MHZCAP     0x20
#define PCI_STATUS_RES1         0x40
#define PCI_STATUS_FB2BCAP      0x80
#define PCI_STATUS_MDPE         0x100
#define PCI_STATUS_DEVSELTIMING 0x600
#define PCI_STATUS_STA          0x800
#define PCI_STATUS_RTA          0x1000
#define PCI_STATUS_RMA          0x2000
#define PCI_STATUS_SSE          0x4000

/* These are for base address registers */
#define PCI_BAR_IS_IO(x)     (x & 1)
#define PCI_BAR_IO_ADDR(x)   (x & 0xFFFFFFFC)
#define PCI_BAR_IS_MEM(x)    !PCI_BAR_IS_IO(x)
#define PCI_BAR_MEM_ADDR(x)  (x & 0xFFFFFFF0)
#define PCI_BAR_MEM_SIZE(x)  ((x & 0x00000006) >> 1)
#define PCI_BAR_MEM_PREFETCH 0x8

#define PCI_BAR_MEM_SIZE32  0x00
#define PCI_BAR_MEM_SIZE64  0x02

#endif

