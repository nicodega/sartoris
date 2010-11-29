/*  
 *   Sartoris microkernel arch-neutral paging functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */


#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h"
#include "sartoris/scr-print.h"
#include "lib/message.h"
#include "lib/shared-mem.h"
#include "lib/bitops.h"
#include "lib/indexing.h"
#include <sartoris/critical-section.h>
#include "sartoris/kernel-data.h"
#include "sartoris/error.h"

/* paging implementation */
int grant_page_mk(void *physical)
{
	int ret = FAILURE;
#ifdef PAGING
	int x = mk_enter();

	if(dyn_pg_lvl == DYN_PGLVL_NONE && dyn_pg_nest == DYN_NEST_ALLOCATING)
	{
		ret = arch_grant_page_mk(physical);
		/* 
			We will return to the process which generated the need 
			for dynamic memory in the first place. 
		
			If interrupt is nesting, pop it from the stack, if not, just 
			issue run_thread
		*/
		dyn_remaining--;

		if(dyn_remaining == 0)
			dyn_pg_nest = DYN_NEST_ALLOCATED; // set pg_nest to DYN_NEST_ALLOCATED so run_thread wont fail

        set_error(SERR_OK);
		if(int_nesting[PAGE_FAULT_INT])  // PAGE_FAULT_INT comes from ARCH
			ret_from_int();
		else
			run_thread(dyn_pg_thread);
	}
    else
    {
        set_error(SERR_NOT_ALLOCATING);
    }
	
	mk_leave(x);
#endif	
	return ret;
}

int page_in(int task, void *linear, void *physical, int level, int attrib) 
{
#ifndef PAGING
    set_error(SERR_NO_PAGING);
	return FAILURE;
#else
	struct task *stask;
	int x = mk_enter(), ret = FAILURE;
    
	/*
	NEW: We MUST check task was created! This could be done on inverse
	order before, because page directories where already allocated. 
	*/
	if(0 <= task && task < MAX_TSK && TST_PTR(task,tsk))
	{
		stask = GET_PTR(task,tsk);
		if(stask->state == ALIVE)
		{
            ret = arch_page_in(task, linear, physical, level, attrib);
            if(ret == SUCCESS)
                set_error(SERR_OK);
            else
                set_error(SERR_ERROR);
		}
        else
        {
            set_error(SERR_INVALID_TSK);
        }
	}
    else
    {
        if(0 > task || task >= MAX_TSK)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_TSK);
    }
	
	mk_leave(x);
	
	return ret;
#endif
}

int page_out(int task, void *linear, int level) {
#ifndef PAGING
    set_error(SERR_NO_PAGING);
    return FAILURE;
#else
	struct task *stask;
	int x = mk_enter(), ret = FAILURE;

	/*
	NEW: We MUST check task was created! This could be done on inverse
	order before, because page directories where already allocated.
	*/
	if((0 <= task) && (task < MAX_TSK) && TST_PTR(task,tsk))
	{
		stask = GET_PTR(task,tsk);
		if(stask->state == ALIVE)
		{
			ret = arch_page_out(task, linear, level);
            if(ret == SUCCESS)
                set_error(SERR_OK);
            else
                set_error(SERR_ERROR);
		}
        else
        {
            set_error(SERR_INVALID_TSK);
        }
	}
    else
    {
        if(0 > task || task >= MAX_TSK)
            set_error(SERR_INVALID_ID);
        else
            set_error(SERR_INVALID_TSK);
    }
	
	mk_leave(x);
	
	return ret;
#endif
}

int flush_tlb() {
#ifdef PAGING
    set_error(SERR_OK);
    return arch_flush_tlb();
#else
    set_error(SERR_NO_PAGING);
    return FAILURE;
#endif
}

int get_page_fault(struct page_fault *pf) 
{
	int result;
	int x;

	result = FAILURE;

#ifdef PAGING

	if (last_page_fault.task_id != -1 || dyn_pg_lvl != DYN_PGLVL_NONE) 
	{		
		x = mk_enter(); /* enter critical block */

		pf = MAKE_KRN_PTR(pf);

		if (VALIDATE_PTR(pf) && VALIDATE_PTR(pf + sizeof(struct page_fault) - 1)) 
		{
			*pf = last_page_fault;
            set_error(SERR_OK);
			result = SUCCESS;
		}
        else
        {
            set_error(SERR_INVALID_PTR);
        }

		mk_leave(x); /* exit critical block */
	}
    else
    {
        set_error(SERR_NO_PAGEFAULT);
    }
  
#endif

	return result;

}
