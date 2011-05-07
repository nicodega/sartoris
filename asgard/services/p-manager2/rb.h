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


#ifndef RB_H
#define RB_H

#include "types.h"

struct _rbnode
{
    UINT32 value;                       // Memory address the thread is waiting for.
	UINT32 value2;                      // we will allow a second value on the nodes
	struct _rbnode *link[0];            // 0 Left node, 1 right
	struct _rbnode *parent;             // parent node
	int color;                          // color of the node
    struct _rbnode *next;	            // used for nodes with the same value
    struct _rbnode *prev;
};

struct _rbtree
{
    rbnode *root;
    rbnode *max;
    rbnode *min;
}

typedef struct _rbtree rbt;
typedef struct _rbnode rbnode;

void rb_init(rbt *t);
rbnode *rb_search(rbt *t, UINT32 value);
int rb_insert(rbt *t, rbnode *n, BOOL insert_only_if_found);
void rb_remove_child(rbt *t, rbnode *n);
void rb_remove(rbt *t, rbnode *n);
BOOL rb_free_value(rbt *t, UINT32 *value);
BOOL rb_inorder(rbt *t, void (*callback)(rbnode *n));

#endif


