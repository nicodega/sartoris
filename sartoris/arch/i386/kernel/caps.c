
#include "caps.h"

struct arch_capabilities arch_caps;

int arch_caps_hascpuid()
{
	unsigned int ret = 0;

	__asm__ __volatile__ (
		/* is CPUID instruction supported?  */
		"pushf\n\t"
		"popl %%eax\n\t"
		"movl %%eax, %%ecx\n\t"

		/* set ID bit */
		"xorl %1, %%eax\n\t"
		"push %%eax\n\t"
		"popf\n\t"

		/* get EFLAGS (should have ID set) */
		"pushf\n\t"
		"popl %%eax\n\t"

		/* compare */
		"xorl %%ecx, %%eax\n\t"
		"andl %1,%%eax\n\t"
		
		"mov %%eax, %0": "=m" (ret): "i" (SCAP_EFLAGS_ID) : "%eax","%ecx");
	
	return ret;
}

void arch_caps_init()
{
	unsigned int flags, flags2, amd_flags;

	arch_caps.caps_tested = 0;
	arch_caps.flags = 0;
	arch_caps.except_raised = 0;
	arch_caps.cores = 0;

	/*
	Do we have CPUID? (I'll assume we are on a 486+ system)
	*/
	if(arch_caps_hascpuid())
	{
		/*
			Find out capabilities based on CPUID instruction and CRX registers
		*/
		__asm__ __volatile__ (

		"xorl %%eax, %%eax\n\t"
		"cpuid\n\t"

		/* Check if we have AMD or Intel processor */
		
		/* Check for Intel */
		"cmpl $0x756e6547, %%ebx\n\t"
		"jne notIntel\n\t"
		"cmpl $0x49656e69, %%edx\n\t"
		"jne notIntel\n\t"
		"cmpl $0x6c65746e, %%ecx\n"
		"jne notIntel\n\t"
		"jmp cpuIntel\n\t"

		/* Check for AMD */
		"notIntel:\n\t"
		"cmpl $0x68747541, %%ebx\n\t"
		"jne standardCPUID\n\t"
		"cmpl $0x69746e65, %%edx\n\t"
		"jne standardCPUID\n\t"
		"cmpl $0x444d4163, %%ecx\n"
		"jne standardCPUID\n\t"
		
		/* AMD cpu, use extended cpuid for 3DNow! */ 
		"movl $0x80000001, %%eax\n\t"
		"cpuid\n\t"
		"movl %%edx, %3\n\t"
		
		/* Standard Intel CPUID */
		"standardCPUID:\n\t"
		"cpuIntel:\n\t"
		/* Try eax=1 */
		"movl %%eax, %%ebx\n\t"
		"cmpl $1, %%ebx\n\t"
		"jb _arch_caps_init_cpuid_end\n\t"
		"movl $1, %%eax\n\t"
		"cpuid\n\t"
		"movl %%ebx, %%eax\n\t"
		"shrl $16, %%eax\n\t"
		"andl $0xFF, %%eax\n\t"
		"movl %%eax, %0\n\t"
		"movl %%ecx, %2\n\t"
		"movl %%edx, %1\n\t"
		"_arch_caps_init_cpuid_end:"  : : "m" (arch_caps.cores), "m" (flags), "m" (flags2), "m" (amd_flags) : "%eax","%ebx","%edx","%ecx");	

	    /* Translate flags */
		if(flags & CPUID_SCAP_FPU) arch_caps.flags |= SCAP_FPU;
		if(flags & CPUID_SCAP_DE) arch_caps.flags |= SCAP_DE;
		if(flags & CPUID_SCAP_TSC) arch_caps.flags |= SCAP_TSC;
		if(flags & CPUID_SCAP_MMX) arch_caps.flags |= SCAP_MMX;
		if(flags & CPUID_SCAP_FXSR) arch_caps.flags |= SCAP_FXSR;
		if(flags & CPUID_SCAP_SSE) arch_caps.flags |= SCAP_SSE;
		if(flags & CPUID_SCAP_SSE2) arch_caps.flags |= SCAP_SSE2;
		if(flags & CPUID_SCAP_TM) arch_caps.flags |= SCAP_TM;
		if(flags2 & CPUID_SCAP_SSE3) arch_caps.flags |= SCAP_SSE3;
		if(flags2 & CPUID_SCAP_MONITOR) arch_caps.flags |= SCAP_MONITOR;
		if(flags2 & CPUID_SCAP_TM2) arch_caps.flags |= SCAP_TM2;
		if(flags2 & CPUID_SCAP_MSR) arch_caps.flags |= SCAP_MSRS;
		if(flags2 & CPUID_SCAP_CLFLUSH) arch_caps.flags |= SCAP_CLFLUSH;
		if(amd_flags & CPUID_SCAP_3DNOW) arch_caps.flags |= (SCAP_3DNOW | SCAP_MMX);
		
		/*
		Setup CR0 and CR4 specifying our caps
		*/
		// set cr4 TSD bit to 0 if we want to provide RDTSC to everyone
		if(arch_has_cap_or(SCAP_TSC))
		{
			__asm__ __volatile__ ("movl %%cr4, %%eax; orl %0, %%eax; movl %%eax,%%cr4" : : "i" (SCAPS_CR4_TSD): "%eax");
		}

		// set PCE on cr4 to 1 if we want everyone to be able to issue RDPMC
		if(arch_has_cap_or(SCAP_MSRS))
		{
			__asm__ __volatile__ ("movl %%cr4, %%eax; orl %0, %%eax; movl %%eax,%%cr4" : : "i" (SCAPS_CR4_PCE): "%eax");
		}
		
		// set DE on cr4 if debugging extensions are present to 0
		if(arch_has_cap_or(SCAP_DE))
		{
			__asm__ __volatile__ ("movl %%cr4, %%eax; andl %0, %%eax; movl %%eax,%%cr4" : : "i" (~SCAPS_CR4_DE): "%eax");
		}

#ifdef FPU_MMX
		// OSFXSR on cr4 indicates we support FXSTOR and FXSAVE on OS
		if(arch_has_cap_or(SCAP_FXSR))
			__asm__ __volatile__ ("movl %%cr4, %%eax; orl %0, %%eax; movl %%eax,%%cr4" : : "i" (SCAPS_CR4_OSFXSR): "%eax");
		else
			__asm__ __volatile__ ("movl %%cr4, %%eax; andl %0, %%eax; movl %%eax,%%cr4" : : "i" (~SCAPS_CR4_OSFXSR): "%eax");

		/* 
		Initialize FPU 
		We wont provide emulation for FPU, so set EM to 0 (this has also to be this way for MMX if present)
		MP will be set to produce an exception
		NE will be set to be handled by an external interrupt (dont know if this should be like this)
		*/
		if(arch_has_cap_or(SCAP_FPU))
		{
			__asm__ __volatile__ ("movl %%cr0, %%eax; orl %0, %%eax; andl %1, %%eax;movl %%eax,%%cr0" : : "i" (SCAPS_CR0_MP), "i" (~(SCAPS_CR0_NE | SCAPS_CR0_EM)): "%eax");
		}
		else
		{
			/* If FPU is not present, we will derive the exception to the OS */
			__asm__ __volatile__ ("movl %%cr0, %%eax; orl %0, %%eax; andl %1, %%eax;movl %%eax,%%cr0" : : "i" (SCAPS_CR0_MP), "i" (~(SCAPS_CR0_NE | SCAPS_CR0_EM)): "%eax");
		}

		/* 
		Initialize MMX 
		Intel does not say anything about it's initialization... so don't do anything
		*/

		/* Initialize SSE/SSE2/SSE3 */
		if(arch_has_cap_and(SCAP_CLFLUSH | SCAP_FXSR))
		{
			if(arch_has_cap_or(SCAP_SSE | SCAP_SSE2 | SCAP_SSE3))
			{
				// set SCAPS_CR4_OSMMEXCPT indicating OS
				// handles SSE floating point exceptions
				__asm__ __volatile__ ("movl %%cr4, %%eax; orl %0, %%eax; movl %%eax,%%cr4" : : "i" (SCAPS_CR4_OSMMEXCPT): "%eax");

				// Set operating mode on MXCSR
				// RC (Rounding control) will be left to 00 (round to nearest)
				// FZ (Flush to zero) will remain disabled
                // DZ (denormal zeros will also be disabled)
				// All exceptions are unmasked... this means if one of them happens
				// the OS on top of sartoris will have to handle them.
				arch_caps.initial_mxcsr = 0;
				__asm__ __volatile__ ("ldmxcsr %0" ::"m" (arch_caps.initial_mxcsr): "%eax"); // NOTE: This will also be set upon first execution of a thread
			}			
		}
		
#endif		
	}

	arch_caps.caps_tested = 1;
	arch_caps.except_raised = 0;
}

/* 
Test if a given capability is present.
Returns: 0 if cap is not present, non 0 otherwise.
NOTE: If caps are ORed together, I'll check for at least one
*/
int arch_has_cap_or(int caps)
{
	return (arch_caps.flags & caps);
}

/* 
Test if a given capability is present.
Returns: 0 if cap is not present, 1 otherwise.
NOTE: This variant of has cap will return 1 if ALL caps are present
*/
int arch_has_cap_and(int caps)
{
	return (arch_caps.flags & caps) == caps;
}

/*
Will be invoked when a capabilites exception is raised.
Returns 1 if exception must be raised, 2 if exception must be ignored, 0 if exception will be hadled by state manager.
*/
int caps_exception()
{
	if(arch_caps.caps_tested == 0)
	{
		arch_caps.except_raised = 1;
		return 2; // ignore
	}
	if(!arch_has_cap_or(SCAP_MMX | SCAP_FPU | SCAP_SSE | SCAP_SSE2 | SCAP_SSE3 | SCAP_FXSR)) 
		return 1;  // we don't have these
	return 0; // must be handled by state management
}
