
#define SCAP_EFLAGS_ID      0x200000

// AMD extended CPUID 3DNow
#define CPUID_SCAP_3DNOW    0x80000000

// CPUID FLAGS 1
#define CPUID_SCAP_FPU      0x1
#define CPUID_SCAP_DE       0x4
#define CPUID_SCAP_TSC      0x10
#define CPUID_SCAP_MSR      0x20
#define CPUID_SCAP_MMX      0x800000
#define CPUID_SCAP_FXSR     0x1000000
#define CPUID_SCAP_SSE      0x2000000
#define CPUID_SCAP_SSE2     0x4000000
#define CPUID_SCAP_TM       0x20000000

// CPUID FLAGS2 (real flags bits are FLAG - 1)
#define CPUID_SCAP_SSE3     0x3
#define CPUID_SCAP_MONITOR  0x9
#define CPUID_SCAP_TM2      0x101
#define CPUID_SCAP_CLFLUSH  0x80000

// INTERNAL FLAGS
#define SCAP_FPU      0x1
#define SCAP_DE       0x2
#define SCAP_TSC      0x4
#define SCAP_MMX      0x8
#define SCAP_FXSR     0x10
#define SCAP_SSE      0x20
#define SCAP_SSE2     0x40
#define SCAP_SSE3     0x80
#define SCAP_TM       0x100
#define SCAP_MONITOR  0x200
#define SCAP_TM2      0x400
#define SCAP_MSRS     0x800
#define SCAP_CLFLUSH  0x1000
#define SCAP_3DNOW    0x2000

// CR4 defines
#define SCAPS_CR4_TSD        0x4
#define SCAPS_CR4_DE         0x8
#define SCAPS_CR4_PCE        0x100
#define SCAPS_CR4_OSFXSR     0x200
#define SCAPS_CR4_OSMMEXCPT  0x400

// CR0 defines 
#define SCAPS_CR0_EM      0x4
#define SCAPS_CR0_MP      0x2
#define SCAPS_CR0_NE      0x20

// MXCSR defines (SIMD)
#define MXCSR_IE          0x1
#define MXCSR_DE          0x2
#define MXCSR_ZE          0x4
#define MXCSR_OE          0x8
#define MXCSR_UE          0x10
#define MXCSR_PE          0x20
#define MXCSR_DAZ         0x40
#define MXCSR_IM          0x80
#define MXCSR_DM          0x100
#define MXCSR_ZM          0x200
#define MXCSR_OM          0x400
#define MXCSR_UM          0x800
#define MXCSR_PM          0x1000
#define MXCSR_RC0         0x2000
#define MXCSR_RC1         0x4000
#define MXCSR_FZ          0x8000


struct arch_capabilities
{
	unsigned int flags;
	unsigned int caps_tested;      // Once capabilities have been tested this will be set to 1
	unsigned int except_raised;      // if an exception is rised this will be set
	unsigned int cores;
	unsigned int initial_mxcsr;
};


/* 
Test if a given capability is present.
Returns: 0 if cap is not present, 1 otherwise.
*/
int arch_has_cap_or(int caps);
int arch_has_cap_and(int caps);
/*
Will be invoked when a capabilites exception is raised.
Returns 0 if exception must NOT be raised, 1 otherwise.
*/
int caps_exception();


