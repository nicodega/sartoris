/*
*	Process and Memory Manager Service
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
#include "vm.h"
#include "mem_layout.h"
#include "vmm_phy_alloc.h"
#include "types.h"

void pya_insert(phy_allocator *pa, phy_page_node *a, int pow, int list);
BOOL pya_check_buddies(phy_page_node *a, int pow, phy_page_node **pj);

/*
This function will init the page memory allocator.
*/
void pya_init(phy_allocator *pa) 
{
    int i = 0;
    for(;i < PYA_SIZES; i++)
    {
	    pa->pg_index[i] = NULL;
        pa->pg_index_prot[i] = NULL;  
    }
}

void pya_put_page(phy_allocator *pa, ADDR pg_laddr, int list)
{
    pya_insert(pa, (phy_page_node*)pg_laddr, 0, list);
}

void pya_put_pages(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, int list)
{
    pg_laddr = (void*)((UINT32)pg_laddr + ((pages-1) << 12));

    while(pages)
    {
        pya_insert(pa, (phy_page_node*)pg_laddr, 0, list);
        pages--;
        pg_laddr = (void*)((UINT32)pg_laddr - 0x1000);
    }
}

void pya_insert(phy_allocator *pa, phy_page_node *a, int pow, int list)
{
    // a will always be a head.
    phy_page_node *pj = NULL, *oa = a;                   // area to join this area with
    struct phy_page_node **lst = ((list == PHY_LIST_COMMON)? pa->pg_index : pa->pg_index_prot);

    a->index = 0;
    a->type = PHY_NTYPE_HEAD;
    a->prev = a->next = NULL;
    a->half_child[pow] = a->first_child = a->last_child = a;
    a->list = list;
    a->parent = a;
    
    // check this page buddies. 
    // - Buddies are on the index? -> promote them and do the same
    // - Buddies are not on the index -> place the area on the index
    while(pya_check_buddies(a, pow, &pj))
    {
        // remove a from the index
        if(pj->next)
            pj->next->prev = pj->prev;
        if(pj->prev)
            pj->prev->next = pj->next;
        else
            lst[pow] = pj->next;

        // if pya_check_buddies returned TRUE, we must perform a join and promote the area
        if((UINT32)pj < (UINT32)a)
        {
            // join area is on the left
            a->prev = pj->last_child;
            a->type = PHY_NTYPE_MIDDLE;
            a->parent = pj;
            pj->last_child->next = a;
            if(pj->last_child != pj)
                pj->last_child->type = PHY_NTYPE_MIDDLE;
            pj->last_child = a->last_child;
            pj->half_child[pow+1] = a;
            if(pj->first_child == pj) 
                pj->first_child = a;

            a = pj;
        }
        else
        {   
            // join area is on the right
            pj->prev = a->last_child;
            pj->type = PHY_NTYPE_MIDDLE;
            pj->parent = a;
            a->last_child->next = pj;
            if(a->last_child != a)
                a->last_child->type = PHY_NTYPE_MIDDLE;
            a->last_child = pj->last_child;
            a->half_child[pow+1] = pj;
            if(a->first_child == a) 
                a->first_child = pj;
        }
        pow++;
    }

    // place the area on the index
    a->next = lst[pow];
    if(a->next) 
        a->next->prev = a;
    a->prev = NULL;
    a->index = pow;
    if(a->last_child != a)
        a->last_child->type = PHY_NTYPE_TAIL;
    lst[pow] = a;

    // set taken
    struct taken_table *ttable = vmm.taken.tables[PM_LINEAR_TO_DIR(oa)];
    ttable->entries[PM_LINEAR_TO_TAB(oa)].data.b = 0;
}

BOOL pya_check_buddies(phy_page_node *a, int pow, phy_page_node **pj)
{
    /*
    We need to check areas to the left and right
    on the taken structure. If they are not taken,
    get the page and check the index member and type.
    If it's on the same index and it's a head or tail
    we can merge them.
    */
    unsigned int size = 0;
    struct taken_table *ttable = NULL;
    phy_page_node *b = NULL;

    if(pow == PYA_SIZES-1) return FALSE;

    size = ((0x1 << pow) << 12);

    // try left
    if((UINT32)a > size)
    {
        ttable = vmm.taken.tables[PM_LINEAR_TO_DIR((UINT32)a - size)];
	    if(ttable->entries[PM_LINEAR_TO_TAB((UINT32)a - size)].data.b_pg.taken == 0)
        {
            // it's not taken, we can buddy up with it!
            b = (phy_page_node *)((UINT32)a - size);

            if(b->type == PHY_NTYPE_HEAD && b->index == pow && b->list == a->list)
            {
                *pj = b;
                return TRUE;
            }
        }
    }

    // try right
    if((UINT32)a + size > (UINT32)a)    // check for overflow
    {
        ttable = vmm.taken.tables[PM_LINEAR_TO_DIR((UINT32)a + size)];
	    if(ttable->entries[PM_LINEAR_TO_TAB((UINT32)a + size)].data.b_pg.taken == 0)
        {
            // it's not taken, we can buddy up with it!
            b = (phy_page_node *)((UINT32)a + size);

            if(b->type == PHY_NTYPE_HEAD && b->index == pow && b->list == a->list)
            {
                *pj = b;
                return TRUE;
            }
        }
    }

    return FALSE;
}

phy_page_node *pya_get_pages_simple(phy_allocator *pa, int pages, int list);

void *pya_get_page(phy_allocator *pa, int list)
{
    return pya_get_pages_simple(pa, 1, list);
}

phy_page_node *pya_get_pages_simple(phy_allocator *pa, int pages, int list)
{
    int pow = (32 -__builtin_clz(pages))-1;
    phy_page_node *a = NULL;
    phy_page_node *b = NULL;
    struct phy_page_node **lst = ((list == PHY_LIST_COMMON)? pa->pg_index : pa->pg_index_prot);

    // check if we have an area with the appropiate size
    if(pow >= PYA_SIZES) return NULL;

    if(lst[pow])
    {
        a = lst[pow];
        lst[pow] = a->next;
        if(lst[pow]) lst[pow]->prev = NULL;

        return a;
    }
    
    pow++;

    // go through the bigger pows and see if we can split
    // one in half
    while(!lst[pow] && pow < PYA_SIZES)
    {
        pow++;
    }

    if(pow == PYA_SIZES) return NULL;

    // remove the area from it's index
    a = lst[pow];
    lst[pow] = a->next;
    if(a->next)
        a->next->prev = NULL;

    // split the area as many times as necesary
    while(pow != 0)
    {
        // keep right half on the index
        b = a->half_child[pow];
        b->next = lst[pow-1];
        if(b->next)
            b->next->prev = b;
        b->prev = NULL;
        lst[pow-1] = b;
        
        b->list = list;
        b->type = PHY_NTYPE_HEAD;

        pow--;
    }

    return a;
}

void *pya_get_pages(phy_allocator *pa, int pages, int list)
{
    phy_page_node *a = NULL,
                  *start = NULL;
    int cpow = (32 -__builtin_clz(pages))-1;
    
    // first try to get a region in the "simple" way
    if(pages == (0x1 << cpow) && (a = pya_get_pages_simple(pa, pages, list)))
        return a;
    else if (pages != (0x1 << cpow))
    {
        if(a = pya_get_pages_simple(pa, (0x1 << (cpow + 1)), list))
        {
            // we got a greater area, return the remaining pages to the allocator
            pya_put_pages(pa, (ADDR)((UINT32)a + ( (0x1 << (cpow + 1)) - pages)), ( (0x1 << (cpow + 1)) - pages), a->list);
            return a;
        }
    }

    // so.. we didn't have a buddy.. we will try the hard way.
    int pow = PYA_SIZES-1;
    unsigned int pgsize = ((pages-1) << 12);
    
    // Starting from the topmost power, I'll try to build
    // the area. This is a really unefficient algorithm,
    // but since we don't have a full buddy with the size
    // we must check the whole structure.
    struct phy_page_node **lst = ((list == PHY_LIST_COMMON)? pa->pg_index : pa->pg_index_prot);

    while(pow >= 0)
    {
        // try each area
        start = lst[pow];

        while(start)
        {
            struct taken_table *ttable = vmm.taken.tables[PM_LINEAR_TO_DIR((UINT32)start + pgsize)];
            if(ttable->entries[PM_LINEAR_TO_TAB((UINT32)start + pgsize)].data.b_pg.taken == 0
                && pya_get_pages(pa, start, pages, list))
                return start;
            start = start->next;
        }
        pow--;
    }
    return NULL;
}

void *pya_get_pages(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, int list)
{
    phy_page_node *fa = (phy_page_node*)pg_laddr;
    phy_page_node *fh = NULL;
    phy_page_node *b = NULL;
    unsigned int i = 0, apgs;
    
    // see if the address is taken
    struct taken_table *ttable = vmm.taken.tables[PM_LINEAR_TO_DIR(pg_laddr)];
    if(ttable->entries[PM_LINEAR_TO_TAB(pg_laddr)].data.b_pg.taken == 1)
        return NULL;

    // find the header of fa's area
    fh = fa;
    while(fh->type != PHY_NTYPE_HEAD)
    {
        fh = fh->parent;        
    }

    i = (((UINT32)fa - (UINT32)fh) >> 12);
    apgs = (((UINT32)fh->last_child - (UINT32)fh) >> 12) + 1;

    // see if we can get protected pages (i.e mmaped)
    if(fh->list == PHY_LIST_PROTECTED && list == PHY_LIST_COMMON)
        return NULL;

    struct phy_page_node **lst = ((fh->list == PHY_LIST_COMMON)? pa->pg_index : pa->pg_index_prot);

    // see how much is left from fa to the end
    if(apgs - i < pages )
    {
        // here we need to check if there is an area with other
        // size which can complete the missing pages (might not have promoted)
        int remaining = pages - (apgs-i);

        // see if we can allocate the remaining pages
        // NOTE: I don't like this being recursive.. but for now 
        // we will implement it like this.
        if(!pya_get_pages(pa, (ADDR)((UINT32)pg_laddr + ((pages - remaining) << 12)), remaining, list))
        {
            return NULL;
        }

        pages -= remaining;
    }
        
    // ok we have enough from fa to the end...
    // remove the area
    if(fh->prev == NULL)
    {
        lst[fh->index] = fh->next;
        if(fh->next) 
            fh->next->prev = NULL;        
    }
    else
    {
        fh->prev->next = fh->next;
        if(fh->next)
            fh->next->prev = fh->prev;
    }
        
    // split at fa (or fa + pages) and put it again
    // on the indexs
    if(fa == fh)
    {
        if( apgs - i == pages) 
            return fa;
        // fa is at the start of the index
        pya_put_pages(pa, (ADDR)((UINT32)fa + (pages << 12)), apgs - pages, ((struct phy_page_node *)((UINT32)fa + (pages << 12)))->list);
    }
    else if(apgs - i == pages )
    {
        // fa is in the middle, but the area we need 
        // ends at the end of the area
        pya_put_pages(pa, (ADDR)fh, apgs - pages, fh->list);
    }
    else
    {
        // fa is in the middle and we need to put pages
        // from bouth sides
        pya_put_pages(pa, (ADDR)fh, i, fh->list);
        pya_put_pages(pa, (ADDR)((UINT32)fa + (pages << 12)), apgs - i - pages, ((struct phy_page_node *)((UINT32)fa + (pages << 12)))->list);
    }

    return fa;
}
