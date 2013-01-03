/*  
 *   Sartoris microkernel arch-neutral paging functions
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

#include "sartoris/kernel.h"
#include "sartoris/permissions.h"
#include "lib/bitops.h"
#include "lib/indexing.h"
#include "sartoris/error.h"
#include "sartoris/cpu-arch.h"

/*
Permission bitmaps handling.
*/

void init_perms(struct permissions *perms)
{
    perms->start = 0;
    perms->length = 0;
    perms->bitmap = NULL;
}

// Validate permissions pointer and structure.
// perms is a user space linear address pointer page aligned (NOT mapped to kernel space).
int validate_perms_ptr(struct permissions *perms, struct permissions *perms_dest, int max, int task)
{
    struct permissions p;
    unsigned int len, start;
    struct permissions *kperms = MAKE_KRN_PTR(perms);

    if( task == -1 
        && VALIDATE_PTR(perms) 
        && VALIDATE_PTR((unsigned int)perms + 4)
        && ((unsigned int)perms) % PG_SIZE == 0 )
    {
        // this could produce a page fault..
        start = kperms->start;
        len = kperms->length;
    }
    else if (task != -1 
        && VALIDATE_PTR_TSK(task,perms) 
        && VALIDATE_PTR_TSK(task,(unsigned int)perms + 4)
        && ((unsigned int)perms) % PG_SIZE == 0)
    {
        // map the other task perms structure to access it.
        // NOTE: This might return NULL if the page is not present and a page fault was issued.
        while(arch_get_perms(task, kperms, &p) != SUCCESS){}

        start = p.start;
        len = p.length;
    }
    else
    {
        set_error(SERR_INVALID_PTR);
        return FAILURE;
    }
        
    if(len <= BITMAP_SIZE(max) 
        && start < max
        && VALIDATE_PTR((unsigned int)perms + sizeof(unsigned int) + len))
    {
        perms_dest->start = start;
        perms_dest->length = len;
        perms_dest->bitmap = (unsigned int*)((unsigned int)kperms + 2 * sizeof(unsigned int));
		set_error(SERR_OK);
        return SUCCESS;
    }
    else
    {
        set_error(SERR_INVALID_SIZE);
        return FAILURE;
    }
}

int test_permission(int task, struct permissions *perms, unsigned int pos)
{
    if(perms->bitmap == NULL) return FAILURE;

    if(pos < perms->start)
    {
        set_error(SERR_NO_PERMISSION);
        return FAILURE;
    }

    pos -= perms->start;

    unsigned int *b;

    while(!(b = arch_map_perms(task, perms, pos))){}

    // we need to map a different page
    if(!getbit_off( b, getbit_pos(pos - perms->start), pos - perms->start))
    {
        set_error(SERR_NO_PERMISSION);
        return FAILURE;
    }

    set_error(SERR_OK);

    return SUCCESS;
}
