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

#define PYA_SIZES 17    // max area size will be 2^18 pages (1GB)

#define PHY_NTYPE_HEAD        1
#define PHY_NTYPE_TAIL        2
#define PHY_NTYPE_MIDDLE      4

#define PHY_LIST_COMMON       0
#define PHY_LIST_PROTECTED    1

/*
This structure will be on the pages on the allocator. 
we won't use aditional memory for this allocator, except 
for the indexs structure and the taken structure of vmm.
*/
struct phy_page_node
{
    UINT32 index;       // index of the list this page belongs to (buddy size)
    UINT32 type;        // it can be on an area index or be the head or tail of the index
    UINT32 list;        // prot or common?
    struct phy_page_node *next;
    struct phy_page_node *prev;
    struct phy_page_node *parent;
    struct phy_page_node *first_child;
    struct phy_page_node *last_child;
    struct phy_page_node *half_child[PYA_SIZES];
};

typedef struct 
{
    struct phy_page_node *pg_index[PYA_SIZES];
    struct phy_page_node *pg_index_prot[PYA_SIZES]; // this structure is like the other one but contains only
                                                    // the regions already asigned to a device mapped on memory.
} phy_allocator;

void pya_init(phy_allocator *ps);
void pya_put_page(phy_allocator *pa, ADDR pg_laddr, int list);
void pya_put_pages(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, int list);
void *pya_get_page(phy_allocator *pa, int list);
void *pya_get_pages(phy_allocator *pa, ADDR pg_laddr, UINT32 pages, int list);

#endif /* __PAGE_STACK_H */
