/*  
*   Sartoris 0.5 i386 kernel screen driver
*   
*   Copyright (C) 2002-2010, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@gmail.com
*/


#include "lib/indexing.h"
#include "lib/containers.h"
#include "sartoris/cpu-arch.h"
#include "i386.h"
#include "cpu-arch-inline.h"
#include "paging.h"
#include "sartoris/kernel.h"
#include "ttrace.h"
#include "tss.h"
#include "selector.h"
#include "caps.h"

extern struct thr_state *mmx_state_owner;  // defined on state_switch.s

struct fsave_data
{
     unsigned int FCW;  // control word
     unsigned int FSW;  // status
     unsigned int FTW;  // tag word
     unsigned int FPUIP;    // instruction pointer
     unsigned int CS;   // cs (only the first word counts)
     unsigned int DP;   // DP
     unsigned int DS;   // data selector (only the first word counts)
     unsigned char regs[80];
};

struct fxsave_data
{
     unsigned short FCW;    // control word
     unsigned short FSW;    // status word
     unsigned short FTW;    // tag word
     unsigned short FPOP;   // operation 
     unsigned int FPUIP;    // Instruction pointer
     unsigned int CS;       // op selector (only first word)
     unsigned int DP;       // data offset
     unsigned int DS;       // data selector (only first word)
     unsigned int MXCSR;
     unsigned int MXCSRMASK;
     unsigned char regs[128];
     unsigned char xregs[128];
     unsigned char reserved4[224];
};

/*
this file implements the architecture dependant section of ttrace (thread trace).
*/

int arch_ttrace_begin(int thr_id)
{
    if(curr_thread == thr_id) return FAILURE;

    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);

    state->sflags |= (SFLAG_TRACEABLE | SFLAG_TRACE_REQ);

    // state won't be available until the thread is executed.
    // When it's executed, we will raise a debug trap on the
    // first usercode instruction to the host OS.
            
    return SUCCESS;
}

void arch_ttrace_end(int thr_id)
{
    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);

    state->sints = 0;
    // set debug breakpoint registers to disabled
    __asm__ __volatile__ ("movl %%dr7,%%eax;"
                          "and $0xFFFFFF00,%%eax;"
                          "movl %%eax,%%dr7;":: );
    state->sflags &= ~(SFLAG_TRACEABLE | SFLAG_DBG | SFLAG_TRACE_REQ);
    state->sflags |= SFLAG_TRACE_END;
}

int arch_ttrace_reg_size(int reg)
{
    if(reg == REG_ALL_GENERAL)
        return sizeof(struct regs);
    if(reg < MMOFFSET)
        return 4;
#ifdef FPU_MMX
    else if(reg < XMMOFFSET)
        return 10;
    else
        return 16;
#endif

    return 0;
}

int arch_ttrace_set_reg(int thr_id, int reg, void *value)
{
    struct thread *thr = (struct thread *)GET_PTR(thr_id,thr);
    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);
    int ret = FAILURE;

    if(state->stack_winding != NULL)
    {
        if(reg < MMOFFSET && (reg < REG_D0 || reg > REG_D7))
        {
            // don't let them mess with sartoris code or segments
            if((reg == REG_EIP && !VALIDATE_PTR_TSK(thr->task_num,*((unsigned int*)value))) 
                || (reg >= REG_SS && reg < REG_EIP && (*((unsigned int*)value) == KRN_CODE || *((unsigned int*)value) == KRN_DATA_SS)))
            {
                return FAILURE;
            }
            *((unsigned int*)((unsigned int)state->stack_winding + (reg << 2))) = *((unsigned int*)value);
            ret = SUCCESS;
        }
        else if(reg >= REG_D0 && reg <= REG_D7)
        {
            if(reg <= REG_D3 && *((unsigned int*)value) < MIN_TASK_OFFSET)  // cannot place breakpoints on sartoris code
                return FAILURE;
            if(reg == REG_D6)
            {
                *((unsigned int*)value) |= 0xFFFF0FF0; 
            }
            else if(reg == REG_D7)
            {
                if(*((unsigned int*)value) & 0x2AA)     // cannot set global breakpoints
                    return FAILURE;
                // make sure reserved registers are ok
                *((unsigned int*)value) &= ~0xF800;
                *((unsigned int*)value) |= 0x400;       // this possition mus have a 1 (so says intel)
            }
            *((unsigned int*)((unsigned int)state->stack_winding + (reg << 2))) = *((unsigned int*)value);
            ret = SUCCESS;
        }
#ifdef FPU_MMX
        else if(reg < XMMOFFSET)
        {
            if(arch_has_cap_or(SCAP_FXSR))
                *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->regs + 10 * (reg - MMOFFSET))) = *((struct xmmreg*)value);
            else
                *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->regs + 16 * (reg - MMOFFSET))) = *((struct xmmreg*)value);
            // if the thread is the current state owner, we have 
            // to clear it's ownership
            if(state == mmx_state_owner)
                mmx_state_owner = NULL;
            ret = SUCCESS;
        }
        else if(reg <= REG_XMM7)
        {
            if(arch_has_cap_and(SCAP_CLFLUSH | SCAP_FXSR) && arch_has_cap_or(SCAP_SSE | SCAP_SSE2 | SCAP_SSE3))
            {
                *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->xregs + 16 * (reg - MMOFFSET))) = *((struct xmmreg*)value);
                if(state == mmx_state_owner)
                    mmx_state_owner = NULL;
                ret = SUCCESS;
            }
        }
#endif
    }
    
    return ret;
}

int arch_ttrace_get_reg(int thr_id, int reg, void *value)
{
    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);
    int ret = FAILURE;

    if(state->stack_winding != NULL)
    {
        if(reg <= MMOFFSET && (reg < REG_D0 || reg > REG_D7))
        {
            *((unsigned int*)value) =  *((unsigned int*)((unsigned int)state->stack_winding + (reg << 2)));
            ret = SUCCESS;
        }
        else if(reg >= REG_D0 && reg <= REG_D7)
        {
            *((unsigned int*)value) =  *((unsigned int*)((unsigned int)state->stack_winding + (reg << 2)));
            ret = SUCCESS;
        }
#ifdef FPU_MMX
        else if(reg < XMMOFFSET)
        {
            if(arch_has_cap_or(SCAP_FXSR))
                *((struct xmmreg*)value) = *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->regs + 10 * (reg - MMOFFSET)));
            else
                *((struct xmmreg*)value) = *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->regs + 16 * (reg - MMOFFSET)));
            ret = SUCCESS;
        }
        else if(reg <= REG_XMM7)
        {
            if(arch_has_cap_and(SCAP_CLFLUSH | SCAP_FXSR) && arch_has_cap_or(SCAP_SSE | SCAP_SSE2 | SCAP_SSE3))
            {
                 *((struct xmmreg*)value) = *((struct xmmreg*)(((struct fxsave_data*)state->mmx)->xregs + 16 * (reg - MMOFFSET)));
                 ret = SUCCESS;
            }
        }
#endif
    }
    return ret;
}

int arch_ttrace_get_regs(int thr_id, void *ptr_regs)
{
    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);
    struct regs *regs = (struct regs*)ptr_regs;

    if(state->stack_winding != NULL)
    {
        *regs = *state->stack_winding;
    }

    return SUCCESS;
}

int arch_ttrace_set_regs(int thr_id, void *ptr_regs)
{
    struct thr_state *state = GET_THRSTATE_ARCH(thr_id);
    struct regs *regs = (struct regs*)ptr_regs;

    if(state->stack_winding != NULL)
    {
        // validate segments and eip
        if(!VALIDATE_PTR(regs->eip))
            return FAILURE;

        if(regs->ss == KRN_CODE || regs->ss == KRN_DATA_SS) return FAILURE;
        if(regs->ds == KRN_CODE || regs->ds == KRN_DATA_SS) return FAILURE;
        if(regs->es == KRN_CODE || regs->es == KRN_DATA_SS) return FAILURE;
        if(regs->cs == KRN_CODE || regs->cs == KRN_DATA_SS) return FAILURE;
        if(regs->gs == KRN_CODE || regs->gs == KRN_DATA_SS) return FAILURE;
        if(regs->fs == KRN_CODE || regs->fs == KRN_DATA_SS) return FAILURE;

        // validate debug register contents
        if((regs->d7 & 1) && regs->d0 < MIN_TASK_OFFSET) return FAILURE;
        if((regs->d7 & 4) && regs->d1 < MIN_TASK_OFFSET) return FAILURE;
        if((regs->d7 & 16) && regs->d2 < MIN_TASK_OFFSET) return FAILURE;
        if((regs->d7 & 64) && regs->d3 < MIN_TASK_OFFSET) return FAILURE;
        
        // cannot set global breakpoints
        if(regs->d7 & 0x2AA)
            return FAILURE;
        
        // make sure reserved registers are ok
        regs->d7 &= ~0xF800;
        regs->d7 |= 0x400;       // this possition must have a 1 (so says intel)
        regs->d6 |= 0xFFFF0FF0;
    
        *state->stack_winding = *regs;
        
        if(regs->d0 != state->stack_winding->d0
            || regs->d1 != state->stack_winding->d1
            || regs->d2 != state->stack_winding->d2
            || regs->d3 != state->stack_winding->d3
            || regs->d6 != state->stack_winding->d6
            || regs->d7 != state->stack_winding->d7)
        {
            state->sflags |= SFLAG_DBG;
        }
        
        return SUCCESS;
    }
            
    return FAILURE;
}
