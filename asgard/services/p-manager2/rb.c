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

#define RED   1
#define BLACK 0

rbnode *getUncle(rbnode *n);
rbnode *getGrandParent(rbnode *n);
void leftRotate(rbt *t, rbnode *n);
void rightRotate(rbt *t, rbnode *n);
rbnode *singleRotation (rbt *t, rbnode *n, int dir );
rbnode *doubleRotation(rbt *t, rbnode *n, int dir );
void rb_insertFixup(rbt *t, rbnode *n);

rbnode *rb_search(rbt *t, UINT32 value) 
{
	rbnode *current = t->root;
			
	while (current != NULL && current->value != value) 
	{
		if(current->value > value) 
			current = current->link[0];
		else
			current = current->link[1];
	}
		
	if(current != NULL)
        return current;
    else
        return NULL;
}

void rb_init(rbt *t)
{
    t->root = NULL;
    t->min = NULL;
    t->max = NULL;
}

// returns 
// - 1 if item was added
// - 2 if node existed and it was added to the same node
int rb_insert(rbt *t, rbnode *n, BOOL insert_only_if_found) 
{
    rbnode *parent = NULL;
	rbnode *current = t->root;
    
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
		t->root = n;
	} 
	else 
	{
		if (parent->value > n->value)
			parent->link[0] = n;
		else
			parent->link[1] = n;
	}

    // fix max and min
    if(!t->min)
        t->min = t->max = n;
    else if(t->min->value > n->value)
        t->min = n;
    else if(t->max->value < n->value)
        t->max = n;
    
    // fix the tree
	rb_insertFixup(t, n);
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

void rb_insertFixup(rbt *t, rbnode *n) 
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
	(t->root)->color = BLACK;
}

void leftRotate(rbt *t, rbnode *n) 
{
	/* Left rotation maintining coloring */
	rbnode *y = n->link[1];
		
	/* Hang a on n's parent */
	if(n->parent == NULL)
	{
		t->root = y;
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
		t->root = y;
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

void rb_remove_child(rbt *t, rbnode *n)
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
        rb_remove(t, n);
    }
}

// this implements a top-down Red Black t deletion.
void rb_remove(rbt *t, rbnode *n)
{
    // This algorithm implementation was taken (mostly)
    // from an explanation on red black trees by Julienne Walker
    // thanks for that! deletion is really a pain in the ass!
    rbnode head;
    rbnode *q, *p, *g, *s; /* Helpers */
    rbnode *f = NULL;      /* Found item */
    int dir = 1;
 
    s = NULL;

    if ( t->root != NULL ) 
    {
        /* Set up helpers */
        q = &head;
        g = p = NULL;
        q->link[1] = t->root;
 
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
                    p = singleRotation (t, q, dir );
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
                                doubleRotation(t, p, last );
                            else if ( s->link[!last]->color == RED )
                                singleRotation(t, p, last );
 
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
            if(t->min == f && t->max == f)
                t->min = t->max = n->parent;
            else if(t->min == f)
                t->min = n->parent;
            else if(t->max == f)
                t->max = n->parent;

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
        t->root = head.link[1];
        if ( t->root != NULL )
            (t->root)->color = BLACK;
    }
}

/*
Finds a value starting at 0 not on the tree.
This function has O(n) running time. It traverses the tree 
inorder, marking nodes and evaluating a node only when 
all it's left tree has been evaluated.
*/
BOOL rb_free_value(rbt *t, UINT32 *value)
{
    int cand = 0;
    rbnode *n = t->min;
    BOOL fromLeft = 0;

    while(n)
    {
	    if (fromLeft || !n->link[0])
		{
            if( n->value > cand || n == t->max )
            {
			    while(n)
                {
				    if(n->link[0])
					    n->link[0]->color &= ~2;
				    if(n->link[1])
					    n->link[1]->color &= ~2;
				    n->color &= ~2;
				    n = n->parent;
                }
                if(n != t->max)
                {
                    *value = cand;
			        return TRUE;
                }
                else
                {
                    return FALSE;
                }
            }
		    cand = n->value + 1;
        }
	    if(!(n->color & 2))
        {
		    n->color |= 2;
		    if(n->link[0] && !(n->link[0]->color & 2))
            {
			    fromLeft = FALSE;
			    n = n->link[0];
			    continue;
            }
		    if(n->link[1] && !(n->link[1]->color & 2))
			{
                fromLeft = FALSE;
			    n = n->link[1];
			    continue;
            }
        }
	    if(n->parent != NULL)
        {
		    fromLeft = (n->parent->link[0] == n);
		    if( n->link[0] )
			    n->link[0]->color &= ~2;
		    if( n->link[1] )
			    n->link[1]->color &= ~2;
		    n = n->parent;
		}
    }
}

    
/*
Traverses the full tree (inorder) in O(n) time, without using a list,
and invoking the callback for each node.
*/
void rb_inorder(rbt *t, void (*callback)(rbnode *n))
{
    rbnode *n = t->min;
    BOOL fromLeft = 0;

    while(n)
    {
	    if (fromLeft || !n->link[0])
		{
            callback(n);
			
            if( n == t->max )
            {
                while(n)
                {
				    if(n->link[0])
					    n->link[0]->color &= ~2;
				    if(n->link[1])
					    n->link[1]->color &= ~2;
				    n->color &= ~2;
				    n = n->parent;
                }
            }
        }
	    if(!(n->color & 2))
        {
		    n->color |= 2;
		    if(n->link[0] && !(n->link[0]->color & 2))
            {
			    fromLeft = FALSE;
			    n = n->link[0];
			    continue;
            }
		    if(n->link[1] && !(n->link[1]->color & 2))
			{
                fromLeft = FALSE;
			    n = n->link[1];
			    continue;
            }
        }
	    if(n->parent != NULL)
        {
		    fromLeft = (n->parent->link[0] == n);
		    if( n->link[0] )
			    n->link[0]->color &= ~2;
		    if( n->link[1] )
			    n->link[1]->color &= ~2;
		    n = n->parent;
		}
    }
}
