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

#include "types.h"

#ifndef __PAGE_PHYALLOC_H
#define __PAGE_PHYALLOC_H

#define PHY_SIZES            17      // max area size will be 2^18 pages (1GB)
#define PHY_MAX_PAGES        0x40000 // 2^18

#define PHY_NTYPE_HEAD        1
#define PHY_NTYPE_TAIL        2
#define PHY_NTYPE_MIDDLE      4

#define PHY_NONE              0
#define PHY_FREE_IO           1
#define PHY_IO                2
#define PHY_REMAP             4

/*
This structure will be on the pages on the allocator. 
we won't use aditional memory for this allocator, except 
for the indexs structure and the taken structure of vmm.
*/
typedef struct _phy_page_node
{
    UINT32 index;       // index of the list this page belongs to (buddy size)
    UINT32 type;        // it can be on an area index or be the head or tail of the index
    struct _phy_page_node *next;
    struct _phy_page_node *prev;
    struct _phy_page_node *parent;
    struct _phy_page_node *first_child;
    struct _phy_page_node *last_child;
    struct _phy_page_node *half_child[PHY_SIZES];
} phy_page_node;

typedef struct 
{
    phy_page_node *pg_index[PHY_SIZES];
} phy_allocator;

void pya_init(phy_allocator *ps);
void pya_put_page(phy_allocator *pa, ADDR pg_laddr, INT32 flags);
void pya_put_pages(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, INT32 flags);
void *pya_get_page(phy_allocator *pa, BOOL io);
void *pya_get_pages(phy_allocator *pa, UINT32 pages, BOOL io);
void *pya_get_pages_aligned(phy_allocator *pa, UINT32 pages, BOOL io, UINT32 align);
void *pya_get_pages_addr(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, BOOL io);

#endif /* __PAGE_STACK_H */
