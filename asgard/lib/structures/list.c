/*
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

#include <lib/structures/list.h>
#include <lib/malloc.h>


void init(list *lst){
	lst->last = lst->first = NULL;
	lst->total = 0;
}


CPOSITION get_head_position(list *lst){
	return (CPOSITION)lst->first;	
}

CPOSITION get_tail_position(list *lst){
	return (CPOSITION)lst->last;	
}

void *get_next(CPOSITION *it){

	list_node *n;

	n = (list_node*)*it;

	if(n == NULL) return NULL;

	*it = (unsigned long)n->next;

	return n->data; 
}

void *get_prev(CPOSITION *it){

	list_node *n;

	n = (list_node*)*it;
	
	if(n == NULL) return NULL;

	*it = (unsigned long)n->prev;

	return n->data; 
}

void *get_at(CPOSITION it){
	if(it == NULL) return NULL;
	return ((list_node*)it)->data;
}

void bring_to_front(list *lst, CPOSITION it)
{
	// get the element at it and set it as the head of the list //
	list_node* n = (list_node*)it;
	
	if(n->prev == NULL) return; // it's the first already 
	
	n->prev->next = n->next;
	if(n->next != NULL)
	{
		n->next->prev = n;
	}
	else
	{
		lst->last = n->prev;	
	}
	n->next = lst->first;
	n->prev = NULL;
	lst->first->prev = n;
	lst->first = n;
}

void send_to_back(list *lst, CPOSITION it)
{
	// get the element at it and set it as the end of the list //
	list_node* n = (list_node*)it;
	
	if(n->next == NULL) return; // already at the end
	
	n->next->prev = n->prev;
	if(n->prev != NULL)
	{
		n->prev->next = n->next;
	}
	else
	{
		lst->first = n->next;
	}
	lst->last->next = n;
	lst->last = n;
}

void remove_at(list *lst, CPOSITION it){
	if((list_node*)it == lst->first)
	{
		if(lst->first == lst->last)
		{
			// there is only one node and it's the one being deleted
			lst->first = lst->last = NULL;
		}
		else
		{
			lst->first = ((list_node*)it)->next;
		}
		if(lst->first != NULL) lst->first->prev = NULL;
	}else if((list_node*)it == lst->last)
	{
		if(lst->first == lst->last)
		{
			// there is only one node and it's the one being deleted
			lst->first = lst->last = NULL;
		}
		else
		{
			lst->last = ((list_node*)it)->prev;
			lst->last->next = NULL;
		}
		if(lst->last != NULL) lst->last->next = NULL;	
	}else{
		((list_node*)it)->prev->next = ((list_node*)it)->next;
		((list_node*)it)->next->prev = ((list_node*)it)->prev;
	}

	lst->total--;
	free((list_node*)it);
}

void remove_all(list *lst){
	struct slist_node *n;

	while(lst->first != NULL){
		n = lst->first->next;
		if(lst->first->data != NULL) free(lst->first->data);
		free(lst->first);
		lst->first = n;
	}
	lst->total = 0;
	lst->last = NULL;
}

void lstclear(list *lst){
	struct slist_node *n;

	while(lst->first != NULL){
		n = lst->first->next;
		free(lst->first);
		lst->first = n;
	}
	lst->total = 0;
	lst->last = NULL;
}

CPOSITION add_head(list *lst, void *data){
	list_node *n = (list_node*) malloc(sizeof(list_node));

	n->data = data;
	n->next = lst->first;
	n->prev = NULL;
	if(lst->first != NULL) lst->first->prev = n;
	lst->first = n;
	if (lst->last == NULL) lst->last = lst->first;

	lst->total++;
	return (CPOSITION)n;
}

CPOSITION add_tail(list *lst, void *data){
	list_node *n = (list_node*) malloc(sizeof(list_node));

	n->data = data;
	n->next = NULL;
	n->prev = lst->last;
	if(lst->last != NULL) lst->last->next = n;
	lst->last = n;
	if (lst->first == NULL) lst->first = lst->last;

	lst->total++;
	return (CPOSITION)n;
}

void *get_head(list *lst){
	return lst->first->data;
}

CPOSITION insert_after(list *lst, CPOSITION it, void *data){
	list_node *n = (list_node*) malloc(sizeof(list_node));

	n->data = data;
	n->prev = (list_node*)it;

	if((list_node*)it == lst->last){
		((list_node*)it)->next = n;
		lst->last = n;
		n->next = NULL;
	}else{
		n->next = ((list_node*)it)->next;
		n->next->prev = n;
		((list_node*)it)->next = n;
	}

	lst->total++;
	return (CPOSITION)n;
}

CPOSITION insert_before(list *lst, CPOSITION it, void *data){

	list_node *n = (list_node*) malloc(sizeof(list_node));

	n->data = data;
	n->next = (list_node*)it;

	if((list_node*)it == lst->first){
		((list_node*)it)->prev = n;
		lst->first = n;
		n->prev = NULL;
	}else{
		n->prev = ((list_node*)it)->prev;
		n->prev->next = n;
		((list_node*)it)->prev = n;
	}

	lst->total++;
	return (CPOSITION)n;
}

int length(list *lst)
{
	return lst->total;
}

void concat(list *l1, list *l2)
{
	if(l1 == NULL || l2 == NULL || l2->total == 0) return;

	if(l1->total == 0)
	{
		l1->first = l2->first;
		l1->last = l2->last;
		l1->total = l1->total;
		return;
	}
	
	l1->last->next = l2->first;
	l1->total += l2->total;
}

