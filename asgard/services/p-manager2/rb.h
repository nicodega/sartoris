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

#define RED   1
#define BLACK 0

struct _rbnode
{
    UINT32 value;                       // Memory address the thread is waiting for.
	struct _rbnode *link[0];            // 0 Left node, 1 right
	struct _rbnode *parent;             // parent node
	int color;                          // color of the node
    struct _rbnode *next;	            // used for nodes with the same value
    struct _rbnode *prev;
	UINT32 value2;                      // we will allow a second value on the nodes
};

typedef struct _rbnode* rbt;
typedef struct _rbnode rbnode;

rbnode *searchRedBlackTree(rbt *t, UINT32 value);
int insertRedBlackTree(rbt *t, rbnode *n, BOOL insert_only_if_found);
void removeChildRedBlackTree(rbt *t, rbnode *n);
void removeRedBlackTree(rbt *t, rbnode *n);

#endif


