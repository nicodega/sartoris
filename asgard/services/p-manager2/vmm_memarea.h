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


#ifndef MEMAREAS_H
#define MEMAREAS_H

#include "types.h"

struct _memarea_node
{
    UINT32 low;                         // left memory boundary
	UINT32 high;                        // right memory boundary
    UINT32 max;                         // augmentation (maximum "high" from subtree rooted at this node)
    struct _memarea_node *link[0];      // 0 Left node, 1 right
	struct _memarea_node *parent;       // parent node
	int color;                          // color of the node
};

typedef struct _memarea_node ma_node;
typedef struct _memarea_node* memareas;

ma_node *ma_collition(memareas *t, UINT32 low, UINT32 high);
ma_node *ma_search(memareas *t, UINT32 low, UINT32 high);
ma_node *ma_search_point(memareas *t, UINT32 p);
ma_node *rb_search_low(memareas *t, UINT32 low);
int ma_insert(memareas *t, ma_node *n);
void ma_remove(memareas *t, ma_node *n);
BOOL ma_inorder(memareas *t, void (*callback)(ma_node *n));
void ma_overlaps(memareas *t, UINT32 low, UINT32 high, BOOL (*callback)(ma_node *n));

#endif


