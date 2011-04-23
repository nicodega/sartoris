/*
*
*	Copylink[1] (C) 2002, 2003, 2004, 2005
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

// This functions implement a red black tree, which accepts the same 
// value multiple times and places them on a list, if the value already
// existed on the tree

#include "vm.h"
#include "rb.h"
#include "pman_print.h"

rbnode *getUncle(rbnode *n);
rbnode *getGrandParent(rbnode *n);
void leftRotate(rbt *t, rbnode *n);
void rightRotate(rbt *t, rbnode *n);
rbnode *singleRotation (rbt *t, rbnode *n, int dir );
rbnode *doubleRotation(rbt *t, rbnode *n, int dir );
void insertRedBlackTreeFixup(rbt *t, rbnode *n);

rbnode *searchRedBlackTree(rbt *t, UINT32 value) 
{
	rbnode *current = *t;
			
	while (current != NULL && current->value != value) 
	{
		if(current->value > value) 
			current = current->link[0];
		else
			current = current->link[1];
	}
		
	if(current != NULL && current->value == value)
        return current;
    else
        return NULL;
}	

// returns 
// - 1 if item was added
// - 2 if node existed and it was added to the same node
int insertRedBlackTree(rbt *t, rbnode *n, BOOL insert_only_if_found) 
{
    rbnode *parent = NULL;
	rbnode *current = *t;
    
    n->link[0] = NULL;
    n->link[1] = NULL;
    n->next = NULL;
    n->prev = NULL;

    // traverse the tree and find a place for this new node
	while (current != NULL) 
	{
		parent = current;
			
		if (current->value > n->value) 
		{	
			current = current->link[0];
		}
		else
		{
			if(n->value == current->value)
            {

                n->prev = current->next;
                current->next = n;
				return 2;
            }
			current = current->link[1];				
		}
	}
	
    if(insert_only_if_found)
    {
        return 0;
    }

    // color the node red and hang it from the parent
	n->parent = parent;
	n->color = RED;
	
	if(parent == NULL) 
	{ 
		*t = n;
	} 
	else 
	{
		if (parent->value > n->value)
			parent->link[0] = n;
		else
			parent->link[1] = n;
	}

    // fix the tree
	insertRedBlackTreeFixup(t, n);
    return 1;
}

rbnode *getUncle(rbnode *n)
{
    if(n->parent == NULL || n->parent->parent == NULL) 
		return NULL;
	if(n->parent->parent->link[0] == n->parent)
		return n->parent->parent->link[1];
	else
		return n->parent->parent->link[0];
}

rbnode *getGrandParent(rbnode *n)
{
    if(n->parent == NULL || n->parent->parent == NULL) 
			return NULL;
	return n->parent->parent;
}

void insertRedBlackTreeFixup(rbt *t, rbnode *n) 
{
	rbnode *y = NULL;
		
	while (n->parent != NULL && n->parent->color == RED) 
	{
		if (n->parent == getGrandParent(n)->link[0]) 
		{
			y = getUncle(n);
				
			if (y != NULL && y->color == RED) 
			{
				n->parent->color = BLACK;
				y->color = BLACK;

				n = getGrandParent(n);
				if(n != NULL)
					n->color = RED;
			} 
			else 
			{
				if (n == n->parent->link[1]) 
				{
					n = n->parent;
					leftRotate(t, n);
				}
				n->parent->color = BLACK;
				getGrandParent(n)->color = RED;
				rightRotate(t, getGrandParent(n));
			}
		}
		else 
		{
			y = getUncle(n);
				
			if (y != NULL && y->color == RED) 
			{
				n->parent->color = BLACK;
				y->color = BLACK;
				n = getGrandParent(n);
				if(n != NULL)
					n->color = RED;
			} 
			else 
			{
				if (n == n->parent->link[0]) 
				{
					n = n->parent;
					rightRotate(t, n);
				}
				n->parent->color = BLACK;
				getGrandParent(n)->color = RED;
				leftRotate(t, n->parent->parent);
			}
		}
	}

    // save us from a red violation at the root
	(*t)->color = BLACK;
}

void leftRotate(rbt *t, rbnode *n) 
{
	/* Left rotation maintining coloring */
	rbnode *y = n->link[1];
		
	/* Hang a on n's parent */
	if(n->parent == NULL)
	{
		*t = y;
		y->parent = NULL;
	}
	else
	{
		y->parent = n->parent;
		if(n->parent->link[0] == n)
			n->parent->link[0] = y;
		else
			n->parent->link[1] = y;
	}
		
    /* put on the right what was on y's left */
	n->link[1] = y->link[0];
	if(y->link[0] != NULL)
		y->link[0]->parent = n;
		
	/* Hang n on y's left */
	n->parent = y;
	y->link[0] = n;
}
	
void rightRotate(rbt *t, rbnode *n) 
{
	/* Right rotation maintining coloring */
	rbnode *y = n->link[0];
		
	/* Hang a on n's parent */
	if(n->parent == NULL)
	{
		*t = y;
		y->parent = NULL;
	}
	else
	{
		y->parent = n->parent;
		if(n->parent->link[0] == n)
			n->parent->link[0] = y;
		else
			n->parent->link[1] = y;
	}
		
	/* put on the left what was on y's right */
	n->link[0] = n->link[0]->link[1];
	if(y->link[1] != NULL)
		y->link[1]->parent = n;
		
	/* Hang n on y's right */
	n->parent = y;
	y->link[1] = n;
}

rbnode *singleRotation (rbt *t, rbnode *n, int dir )
{    
    rbnode *save = n->link[!dir];

    if(dir == 0)
        leftRotate(t, n);
    else
        rightRotate(t, n);

    n->color = RED;
    save->color = BLACK;

    return save;
}

rbnode *doubleRotation(rbt *t, rbnode *n, int dir )
{
    n->link[!dir] = singleRotation(t, n->link[!dir], !dir );
    return singleRotation(t, n, dir );
}

void removeChildRedBlackTree(rbt *t, rbnode *n)
{
    if(n->next != NULL)
    {
        if(n->prev != NULL)
            n->prev->next = n->next;
        n->next->prev = n->prev;
        n->next = NULL;
        n->prev = NULL;
    }
    else if(n->prev != NULL)
    {
        n->prev->next = n->next;
        if(n->next != NULL)
            n->next->prev = n->prev;
        n->next = NULL;
        n->prev = NULL;
    }
    else
    {
        removeRedBlackTree(t, n);
    }
}

// this implements a top-down Red Black t deletion.
void removeRedBlackTree(rbt *t, rbnode *n)
{
    // This algorithm implementation was taken (mostly)
    // from an explanation on red black trees by Julienne Walker
    // thanks for that! deletion is really a pain in the ass!
    rbnode head;
    rbnode *q, *p, *g, *s; /* Helpers */
    rbnode *f = NULL;      /* Found item */
    int dir = 1;
 
    s = NULL;

    if ( *t != NULL ) 
    {
        /* Set up helpers */
        q = &head;
        g = p = NULL;
        q->link[1] = *t;
 
        /* Search and push a red down */
        while ( q->link[dir] != NULL ) 
        {
            int last = dir;
 
            /* Update helpers */
            g = p, p = q;
            q = q->link[dir];
            dir = q->value < n->value;
 
            /* Save found node */
            if ( q->value == n->value )
                f = q;
 
            /* Push the red node down */
            if ( q->color == BLACK && q->link[dir]->color == BLACK ) 
            {
                if ( q->link[!dir]->color == RED )
                {
                    p = p->link[last] = singleRotation (t, q, dir );
                }
                else if ( q->link[!dir]->color == BLACK ) 
                {
                    s = p->link[!last];
 
                    if ( s != NULL ) 
                    {
                        if ( s->link[!last]->color == BLACK && s->link[last]->color == BLACK ) 
                        {
                            /* Color flip */
                            p->color = BLACK;
                            s->color = RED;
                            q->color = RED;
                        }
                        else 
                        {
                            int dir2 = g->link[1] == p;
 
                            if ( s->link[last]->color == RED )
                                g->link[dir2] = doubleRotation(t, p, last );
                            else if ( s->link[!last]->color == RED )
                                g->link[dir2] = singleRotation(t, p, last );
 
                            /* Ensure correct coloring */
                            q->color = g->link[dir2]->color = RED;
                            g->link[dir2]->link[0]->color = BLACK;
                            g->link[dir2]->link[1]->color = BLACK;
                        }
                    }
                }
            }
        }
 
        /* Replace and remove if found */
        if ( f != NULL ) 
        {
            // remove every thread from the list.
            s = f->next;
            while(s)
            {
                g = s->next;
                s->next = NULL;
                s->prev = NULL;
                s = g;
            }

            f->value = q->value;
            f->next = q->next;
            f->prev = NULL;
            f->parent = q->parent;
            p->link[p->link[1] == q] = q->link[q->link[0] == NULL];            
        }
 
        /* Update root and make it black */
        *t = head.link[1];
        if ( *t != NULL )
            (*t)->color = BLACK;
    }
}
