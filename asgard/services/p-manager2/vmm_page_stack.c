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
// REMOVE
#include "vm.h"
#include "mem_layout.h"

#include "page_stack.h"
#include "types.h"

void init_page_stack(struct page_stack *ps) 
{
	ps->top_page = NULL;
	ps->page_count = 0;
	ps->top_page_entries = 0;
}

void push_page(struct page_stack *ps, ADDR new_page) 
{
	int i = 0,*ptr = (int*)new_page;
	if((UINT32)new_page < FIRST_PAGE(PMAN_POOL_LINEAR))
		pman_print_and_stop("PUSHING TOO LOW PAGE");

	/* if we have some page structure already loaded up, */
	if(ps->top_page) 
	{
		/* if we have room in current top page container, */
		if (ps->top_page_entries < PS_ENTRIES_PER_PAGE) 
		{
			/* add new page to current top page container */
			ps->top_page->pages[ ps->top_page_entries++ ] = (page)new_page; 
		} 
		else
		{
			/* have to make room, patch up new top structure */
			struct page_container *pc = (struct page_container*) new_page;
			pc->next = ps->top_page;

			/* hook new structure on top */
			ps->top_page = pc;
			ps->top_page_entries = 0;                       
		}
	}
	else 
	{
		/* list was empty, create and link top container */
		ps->top_page = (struct page_container*) new_page;
		ps->top_page->next = 0; 	// set next to 0
		ps->top_page_entries = 0;    
	}

	ps->page_count++;
}

ADDR pop_page(struct page_stack *ps) 
{
	ADDR ret_page = NULL;

	/* if we have pages at all, */
	if(ps->top_page)
	{
		/* if the current top container has pages, */
		if(ps->top_page_entries > 0) 
		{
			/* return page from top container, decrement top page occupation counter */
			ret_page = (ADDR) ps->top_page->pages[ --ps->top_page_entries ];
		}
		else
		{
			/* top page is empty, return it and move to the next */
			ret_page = (ADDR) ps->top_page;
			ps->top_page = ps->top_page->next;

			/* if there is a next page, */
			if(ps->top_page)
			{
				/* update aforementioned top page occupation counter */
				ps->top_page_entries = PS_ENTRIES_PER_PAGE;
			}
			
			/* if there is no next, we let top_page_entries be */
			/* (it should be zero, since discarded top page was empty) */
		}

		ps->page_count--;
	}

	if(ret_page != NULL)
	{
		UINT32 *p = (UINT32*)ret_page;
		int i = 0;
		for(i=0;i<1024;i++){ p[i] = 0; }
	}
	else
	{
		pman_print_and_stop("PAGE STACK: NULL PAGE RETURN");
	}

	return ret_page;
}

UINT32 page_count(struct page_stack *ps) 
{
	return ps->page_count;
}

/* 
Removes all addresses between lstart and lend from the stack. 
*/
BOOL remove_range(struct page_stack *ps, ADDR lstart, ADDR lend)
{
	/* This could be done some other way, but this is what I came with */
	struct page_stack nps;
	ADDR pg = pop_page(ps);

	init_page_stack(&nps);

	while(pg != NULL)
	{
		if(!(lstart <= pg && lend >= pg))
			push_page(&nps, pg);
	}

	*ps = nps;

	return TRUE;
}

