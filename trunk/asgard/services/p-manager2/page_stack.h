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

#ifndef __PAGE_STACK_H
#define __PAGE_STACK_H

#define PS_ENTRIES_PER_PAGE 1023

typedef UINT32 page_entry;
typedef page_entry *page;

struct page_container
{
  struct page_container *next;
  page   pages[PS_ENTRIES_PER_PAGE];
};

struct page_stack
{
  struct page_container *top_page;
  UINT32 page_count;
  UINT32 top_page_entries;
};

void init_page_stack(struct page_stack *ps);

void push_page(struct page_stack *ps, ADDR new_page);
void *pop_page(struct page_stack *ps);
unsigned int page_count(struct page_stack *ps);
BOOL remove_range(struct page_stack *ps, ADDR lstart, ADDR lend);

#endif /* __PAGE_STACK_H */
