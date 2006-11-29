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

#include "page_list.h"

void init_page_list(page_list *pl) {

  pl->top_page = NULL;
  pl->page_count = 0;
  pl->top_page_entries = 0;

}

void put_page(page_list *pl, void *new_page) 
{

  /* if we have some page structure already loaded up, */
  if (pl->top_page) {

    /* if we have room in current top page container, */
    if (pl->top_page_entries < PL_ENTRIES_PER_PAGE) {

      /* add new page to current top page container */
      pl->top_page->pages[ pl->top_page_entries++ ] = (page) new_page; 

    } else {

      /* have to make room, patch up new top structure */
      page_container *pc = (page_container*) new_page;
      pc->next = pl->top_page;
      /* hook new structure on top */
      pl->top_page = pc;
      pl->top_page_entries = 0;                       
      
    }
  } else {
    
    /* list was empty, create and link top container */
    pl->top_page = (page_container*) new_page;
    pl->top_page->next = 0; 	// set next to 0
    pl->top_page_entries = 0;    

  }

  pl->page_count++;

}

void *get_page(page_list *pl) {
  
  void *ret_page = NULL;
  
  /* if we have pages at all, */
  if (pl->top_page) {
    
    /* if the current top container has pages, */
    if (pl->top_page_entries > 0) {
      /* return page from top container, decrement top page occupation counter */
      ret_page = (void*) pl->top_page->pages[ --pl->top_page_entries ];
    } else {
      /* top page is empty, return it and move to the next */
      ret_page = (void*) pl->top_page;
      pl->top_page = pl->top_page->next;
      
      /* if there is a next page, */
      if (pl->top_page) {
	/* update aforementioned top page occupation counter */
	pl->top_page_entries = PL_ENTRIES_PER_PAGE;
      }
      
      /* if there is no next, we let top_page_entries be */
      /* (it should be zero, since discarded top page was empty) */
      
    }
    
    pl->page_count--;
    
  }

  return ret_page;
}

unsigned int page_count(page_list *pl) {
  return pl->page_count;
}
