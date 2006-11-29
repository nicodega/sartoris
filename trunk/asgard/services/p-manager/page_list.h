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

#ifndef __PAGE_LIST_H
#define __PAGE_LIST_H

#define PL_PAGE_SIZE        4096
#define PL_ENTRIES_PER_PAGE 1023

#ifndef NULL
# define NULL 0
#endif

typedef unsigned int  page_entry;
typedef page_entry* page;

typedef struct page_container_s {
  struct page_container_s *next;
  page   pages[PL_ENTRIES_PER_PAGE];
} page_container;

typedef struct page_list_s {
  page_container *top_page;
  unsigned int page_count;
  unsigned int top_page_entries;
} page_list;

void init_page_list(page_list *pl);
void put_page(page_list *pl, void *new_page);
/* TODO: void put_page_range(page_list *pl, void *start_page, unsigned int num_pages); */
void *get_page(page_list *pl);
unsigned int page_count(page_list *pl);

#endif /* __PAGE_LIST_H */
