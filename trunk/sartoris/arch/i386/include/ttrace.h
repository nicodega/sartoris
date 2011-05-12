/*  
*   Sartoris 0.5 i386 kernel screen driver
*   
*   Copyright (C) 2002-210, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@gmail.com
*/


#ifndef TTRACEH
#define TTRACEH

#define REG_ALL_GENERAL -1

#define REG_SS      0
#define REG_DS      1
#define REG_ES      2
#define REG_CS      3
#define REG_GS      4
#define REG_FS      5
#define REG_D0      6
#define REG_D1      7
#define REG_D2      8
#define REG_D3      9
#define REG_D6      10
#define REG_D7      11
#define REG_EIP     12
#define REG_EDI     13
#define REG_ESI     14
#define REG_EBP     15
#define REG_ESP     16
#define REG_EBX     17
#define REG_EDX     18
#define REG_ECX     19
#define REG_EAX     20
#define REG_EFLAGS  21

#define MMOFFSET    21
#define REG_ST0 (MMOFFSET+1)
#define REG_ST1 (MMOFFSET+2)
#define REG_ST2 (MMOFFSET+3)
#define REG_ST3 (MMOFFSET+4)
#define REG_ST4 (MMOFFSET+5)
#define REG_ST5 (MMOFFSET+6)
#define REG_ST6 (MMOFFSET+7)
#define REG_ST7 (MMOFFSET+8)

#define REG_MM0 (MMOFFSET+1)
#define REG_MM1 (MMOFFSET+2)
#define REG_MM2 (MMOFFSET+3)
#define REG_MM3 (MMOFFSET+4)
#define REG_MM4 (MMOFFSET+5)
#define REG_MM5 (MMOFFSET+6)
#define REG_MM6 (MMOFFSET+7)
#define REG_MM7 (MMOFFSET+8)

#define XMMOFFSET (MMOFFSET+8)
#define REG_XMM0 (XMMOFFSET+1)
#define REG_XMM1 (XMMOFFSET+2)
#define REG_XMM2 (XMMOFFSET+3)
#define REG_XMM3 (XMMOFFSET+4)
#define REG_XMM4 (XMMOFFSET+5)
#define REG_XMM5 (XMMOFFSET+6)
#define REG_XMM6 (XMMOFFSET+7)
#define REG_XMM7 (XMMOFFSET+8)

struct regs
{   
	unsigned int ss;
	unsigned int ds;
	unsigned int es;
	unsigned int cs;
	unsigned int gs;
	unsigned int fs;
    unsigned int d0;
    unsigned int d1;
    unsigned int d2;
    unsigned int d3;
    unsigned int d6;
    unsigned int d7;
	unsigned int eip;
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int esp;
	unsigned int ebx;
	unsigned int edx;
	unsigned int ecx;
	unsigned int eax;
    unsigned int eflags;
};

#ifdef __KERNEL__
struct thr_regs
{    
    unsigned int d0;
    unsigned int d1;
    unsigned int d2;
    unsigned int d3;
    unsigned int d4;
    unsigned int d5;
    unsigned int d6;
    unsigned int d7;
	unsigned int ss;
	unsigned int ds;
	unsigned int es;
	unsigned int cs;
	unsigned int gs;
	unsigned int fs;
	unsigned int eip;
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int esp;
	unsigned int ebx;
	unsigned int edx;
	unsigned int ecx;
	unsigned int eax;
    unsigned int eflags;
};
#endif

struct fpreg
{
    unsigned short sign:1;
    unsigned short exponent:15;
    unsigned int significand_high;
    unsigned int significand_low;
} __attribute__ ((__packed__));

struct xmmreg
{
    unsigned int low;
    unsigned int high;
};

struct xreg
{
    unsigned int r0;
    unsigned int r1;
    unsigned int r2;
    unsigned int r3;
};


#endif
