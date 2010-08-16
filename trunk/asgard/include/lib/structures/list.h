/*
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*    modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#ifndef NULL
#define NULL 0
#endif

#ifndef LISTH
#define LISTH

typedef unsigned long CPOSITION;

struct slist_node{
    void *data;
    struct slist_node *next;
    struct slist_node *prev;
};

typedef struct slist_node list_node;

struct slist{
    list_node *first;
    list_node *last;
    unsigned long total;
};

typedef struct slist list;

void init(list *lst);
CPOSITION get_head_position(list *lst);
CPOSITION get_tail_position(list *lst);
void *get_next(CPOSITION *it);
void *get_prev(CPOSITION *it);
void *get_at(CPOSITION it);
void remove_at(list *lst,CPOSITION it);
CPOSITION add_head(list *lst, void *);
CPOSITION add_tail(list *lst, void *);
void *get_head(list *lst);
void remove_all(list *lst);
void lstclear(list *lst);
CPOSITION insert_after(list *lst, CPOSITION it, void *data);
CPOSITION insert_before(list *lst, CPOSITION it, void *data);
int length(list *lst);
void concat(list *l1, list *l2);
void bring_to_front(list *lst, CPOSITION it);
void send_to_back(list *lst, CPOSITION it);

#endif
