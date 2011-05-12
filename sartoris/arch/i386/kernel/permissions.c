/*  
*   Sartoris 0.5 i386 kernel screen driver
*   
*   Copyright (C) 2002-2010, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@gmail.com
*/

#include "sartoris/cpu-arch.h"
#include "cpu-arch-inline.h"
#include "paging.h"
#include "sartoris/kernel.h"

int arch_get_perms(int task, struct permissions *localperms, struct permissions *p)
{
    struct permissions *map = (struct permissions*)AUX_PAGE_SLOT(curr_thread);

#ifdef PAGING
    if(verify_present(localperms, false) == SUCCESS)
    {
        // page is present
        if(curr_task != task)
        {
            import_page(task, localperms);

            map = (struct permissions*)((unsigned int)map + ((unsigned int)localperms) % PG_SIZE);
            p->length = map->length;
            p->start = map->start;
        }
        else
        {
            localperms = (struct permissions*)MAKE_KRN_PTR(localperms);
            p->length = localperms->length;
            p->start = localperms->start;
        }
    }
    else
    {
        // raise a page fault
        last_page_fault.task_id = task;
		last_page_fault.thread_id = curr_thread;
		last_page_fault.linear = localperms;
        last_page_fault.pg_size = PG_SIZE;
		last_page_fault.flags = PF_FLAG_EXT;

#ifdef _SMP_
		mk_leave(x);
#endif
		arch_issue_page_fault();
#ifdef _SMP_
		x = mk_enter();
#endif
        return FAILURE;
    }
#else
    localperms = (struct permissions*)MAKE_KRN_PTR(localperms);
    p->length = localperms->length;
    p->start = localperms->start;
#endif
    return SUCCESS;    
}

unsigned int *arch_map_perms(int task, struct permissions *perms, unsigned int pos)
{   
#ifdef PAGING
    unsigned int *map = (unsigned int*)AUX_PAGE_SLOT(curr_thread);
    unsigned int *b = perms->bitmap + getbit_pos(pos - perms->start);
    if(verify_present(b, false) == SUCCESS)
    {
        // page is present
        import_page(task, b);
        
        return map;
    }
    else
    {
        // raise a page fault
        last_page_fault.task_id = task;
		last_page_fault.thread_id = curr_thread;
		last_page_fault.linear = perms->bitmap;
        last_page_fault.pg_size = PG_SIZE;
		last_page_fault.flags = PF_FLAG_EXT;

#ifdef _SMP_
		mk_leave(x);
#endif
		arch_issue_page_fault();
#ifdef _SMP_
		x = mk_enter();
#endif
        return NULL;
    }
#else
    return (unsigned int*)((unsigned int)MAKE_KRN_PTR(perms->bitmap) + getbit_pos(pos - perms->start) * 4);
#endif
}

