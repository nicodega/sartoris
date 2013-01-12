/*  
*   Sartoris 0.5 i386 ACPI table definitions for SMP
*   
*   Copyright (C) 2002-210, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@gmail.com
*/

#ifndef ACPI
#define ACPI
#define _SMP_
#ifdef _SMP_

typedef struct {
	char Signature[8];
	unsigned char Checksum;
	char OEMID[6];
	unsigned char Revision;
	unsigned int RsdtAddress;
} RSDPDescriptor;

typedef struct {
	char Signature[4];
	unsigned int Length;
	unsigned char Revision;
	unsigned char Checksum;
	char OEMID[6];
	char OEMTableID[8];
	unsigned int OEMRevision;
	unsigned int CreatorID;
	unsigned int CreatorRevision;
} ACPISDTHeader;

typedef struct {
  ACPISDTHeader h;
} RSDT;

typedef struct {
  ACPISDTHeader *prtSDT;
} RSDT_ENTRY;

struct GenericAddressStructure
{
  unsigned char AddressSpace;
  unsigned char BitWidth;
  unsigned char BitOffset;
  unsigned char AccessSize;
  unsigned int  AddressLow;
  unsigned int  AddressHigh;
};

typedef struct
{
    ACPISDTHeader h;
    unsigned int FirmwareCtrl;
    unsigned int Dsdt;
 
    // field used in ACPI 1.0; no longer in use, for compatibility only
    unsigned char  IntModel;
 
    unsigned char  Reserved;
    unsigned short SCI_Interrupt;
    unsigned int SMI_CommandPort;
    unsigned char  AcpiEnable;
    unsigned char  AcpiDisable;
    unsigned char  S4BIOS_REQ;
    unsigned char  PSTATE_Control;
    unsigned int PM1aEventBlock;
    unsigned int PM1bEventBlock;
    unsigned int PM1aControlBlock;
    unsigned int PM1bControlBlock;
    unsigned int PM2ControlBlock;
    unsigned int PMTimerBlock;
    unsigned int GPE0Block;
    unsigned int GPE1Block;
    unsigned char  PM1EventLength;
    unsigned char  PM1ControlLength;
    unsigned char  PM2ControlLength;
    unsigned char  PMTimerLength;
    unsigned char  GPE0Length;
    unsigned char  GPE1Length;
    unsigned char  GPE1Base;
    unsigned char  CStateControl;
    unsigned short WorstC2Latency;
    unsigned short WorstC3Latency;
    unsigned short FlushSize;
    unsigned short FlushStride;
    unsigned char  DutyOffset;
    unsigned char  DutyWidth;
    unsigned char  DayAlarm;
    unsigned char  MonthAlarm;
    unsigned char  Century;
 
    unsigned short Reserved2;
 
    unsigned char  Reserved3;
    unsigned int Flags;
} FADT;

/*A one indicates that the system also has a PC-AT 
compatible dual-8259 setup. The 8259 vectors must be 
disabled (that is, masked) when enabling the ACPI APIC 
operation.*/
#define MADT_FLAGS_NONE        0 
#define MADT_FLAGS_PCAT_COMPAT 1 

typedef struct {
	ACPISDTHeader h;
	unsigned int localApicAddress;			// The physical address at which each processor can access its local APIC.
	unsigned int flags;						
} MADT;

#define MADT_LAPIC_FLAGS_ENABLED 1			// if 0 the processor is unusable

#define MADT_TYPE_LAPIC       0
#define MADT_TYPE_IOAPIC      1
#define MADT_TYPE_INTOVERRIDE 2

// local apic entry on MADT
typedef struct {
	unsigned char type;			// will be MADT_TYPE_LAPIC
	unsigned char length;		// 8
	unsigned char ACPIProcID;	// The ProcessorId for which this processor is listed in the ACPI Processor declaration operator
	unsigned char APIC_ID;		// The processor‘s local APIC ID
	unsigned int flags;
} MADT_APIC;

// IO APIC entry on MADT
typedef struct {
	unsigned char type;			// will be MADT_TYPE_IOAPIC
	unsigned char length;		// 8
	unsigned char IOAPICID;	    // 
	unsigned char reserved;		// 
	unsigned int address;       // The physical address to access this IO APIC. Each IO APIC resides at a unique address
    unsigned int systemVectorBase;       // The system interrupt vector index where this IO APIC’s INTI lines start. The number of INTI lines is determined by the IO APIC’s Max Redir Entry register.
} MADT_IOAPIC;

// INT override on MADT
typedef struct {
	unsigned char type;			// will be MADT_TYPE_INTOVERRIDE
	unsigned char length;		// 12
	unsigned char bus;			// always 0 - ISA
	unsigned char source;		// bus relative interrupt source
	unsigned int globalSystemInterruptVector;		// The Global System Interrupt Vector that this busrelative interrupt source will trigger
	unsigned short flags;	// 
} MADT_INTOVR;

#define MADT_INTOVR_FLAG_POLARITY(fl) (fl & 3)
#define MADT_INTOVR_FLAG_POLARITY_BUS		0		// for isa it's Low
#define MADT_INTOVR_FLAG_POLARITY_HIGH		1
#define MADT_INTOVR_FLAG_POLARITY_RESERVED	2
#define MADT_INTOVR_FLAG_POLARITY_LOW		3	

#define MADT_INTOVR_FLAG_TRIGMODE(fl) ((fl >> 2) & 3)
#define MADT_INTOVR_FLAG_TRIGMODE_BUS		0		// for isa it's EDGE
#define MADT_INTOVR_FLAG_TRIGMODE_EDGE		1
#define MADT_INTOVR_FLAG_TRIGMODE_RESERVED	2
#define MADT_INTOVR_FLAG_TRIGMODE_LEVEL		3	


RSDPDescriptor *find_RSDP();
int strneq(char *str1, char *str2, int len);
void *findOnRSDP(RSDPDescriptor *RSDP, char *tbl, int len);

#endif

#endif 

