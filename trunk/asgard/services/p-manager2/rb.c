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

#define is_red(n) (n != NULL && n->color == RED)

void rb_left_rotate(rbt *t, rbnode *n);
void rb_right_rotate(rbt *t, rbnode *n);
rbnode *rb_single_rotation (rbnode *n, int dir );
rbnode *rb_double_rotation(rbnode *n, int dir );
void rb_insert_rebalance(rbt *t, rbnode *n);

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
	    t->root->color = BLACK;
	} 
	else 
	{
		if (parent->value > n->value)
			parent->link[0] = n;
		else
			parent->link[1] = n;
    
        // rebalance the tree
	    rb_insert_rebalance(t, n);
	}

    // fix max and min
    if(!t->min)
        t->min = t->max = n;
    else if(t->min->value > n->value)
        t->min = n;
    else if(t->max->value < n->value)
        t->max = n;
    return 1;
}

// This algorithm implementation is a non-recursive adaptation of 
// an explanation on red black trees by Julienne Walker, thanks a lot!
// I thought the implementation was simple enough for all the 
// insertion cases and the code is cleaner and shorter this way.
void rb_insert_rebalance(rbt *t, rbnode *n) 
{
    rbnode *r = n->parent, *p = n;
    int dir;

    // the node n was inserted at the bottom
    // go up fixing the tree
    dir = r->value < n->value;

    while(r && is_red(r->link[dir])) 
    {
        if ( is_red(r->link[!dir]) )
        {
            r->color = RED;
            r->link[0]->color = BLACK;
            r->link[1]->color = BLACK;
        }
        else 
        {
            if ( is_red(r->link[dir]->link[dir]) )
                r = rb_single_rotation( r, !dir );
            else if ( is_red( r->link[dir]->link[!dir] ) )
                r = rb_double_rotation( r, !dir );
        }
                
        p = r;
        r = r->parent;  // go to the upper tree
        if(r)
        {
            r->link[r->value < p->value] = p; // r might have changed, update it on it's parent
            dir = r->value < n->value;
        }
    }

    if(r == NULL)
        t->root = p;
        
    // save us from a red violation at the root
	t->root->color = BLACK;
}

// this function performs a rotation on the specified direction (0 left, 1 right)
rbnode *rb_single_rotation( rbnode *n, int dir )
{
    rbnode *save = n->link[!dir];

    n->link[!dir] = save->link[dir];
    if(n->link[!dir]) 
        n->link[!dir]->parent = n;
    save->link[dir] = n;
    save->parent = n->parent;
    n->parent = save;

    n->color = RED;
    save->color = BLACK;

    return save; // return the new root
}

// this function will perform first a rotation on the oposite direction
// on the node child on that direction and then on the specified
// direction on the node
rbnode *rb_double_rotation(rbnode *n, int dir )
{
    n->link[!dir] = rb_single_rotation(n->link[!dir], !dir );
    if(n->link[!dir])
        n->link[!dir]->parent = n;
    return rb_single_rotation(n, dir );
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
    rbnode head = {0};
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
            if ( !is_red(q) && !is_red(q->link[dir]) ) 
            {
                if ( is_red(q->link[!dir]) )
                {
                    p = p->link[last] = rb_single_rotation (q, dir );
                }
                else if ( !is_red(q->link[!dir]) ) 
                {
                    s = p->link[!last];
 
                    if ( s != NULL ) 
                    {
                        if ( !is_red(s->link[!last]) && !is_red(s->link[last]) ) 
                        {
                            /* Color flip */
                            p->color = BLACK;
                            s->color = RED;
                            q->color = RED;
                        }
                        else 
                        {
                            int dir2 = (g->link[1] == p);
 
                            if ( is_red(s->link[last]) )
                                g->link[dir2] = rb_double_rotation(p, last );
                            else if ( is_red(s->link[!last]))
                                g->link[dir2] = rb_single_rotation(p, last );
 
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
                t->min = t->max = NULL;
            else if(t->min == f)
                t->min = (q == f)? p : q;
            else if(t->max == f)
                t->max = q;

            // remove every thread from the list.
            s = f->next;
            while(s)
            {
                g = s->next;
                s->next = NULL;
                s->prev = NULL;
                s = g;
            }

            /* 
            On the original algorithm f value is replaced with q's one,
            but we cannot do that since our nodes are static (belong to a thread or something)
            */
            s = q->link[q->link[0] == NULL];
            
            q->color = f->color;
            q->link[0] = f->link[0];
            q->link[1] = f->link[1];
            q->parent = f->parent;
                      
            if(q->link[0])
                q->link[0]->parent = q;

            if(q->link[1])
                q->link[1]->parent = q;

            // NOTE: p could be f.
            if(p == f)
                q->link[p->link[1] == q] = s;
            else
                p->link[p->link[1] == q] = s;
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
    unsigned int cand = 0;
    rbnode *n = t->min;
    BOOL fromLeft = 0;

    while(n)
    {
	    if (fromLeft || (!n->link[0] && ! (n->color & 2)))
		{
            if( n->value > cand || n == t->max )
            {
                if(n == t->max && n->value == cand)
                    cand = n->value + 1;

			    while(n)
                {
				    if(n->link[0])
					    n->link[0]->color &= ~2;
				    if(n->link[1])
					    n->link[1]->color &= ~2;
				    n->color &= ~2;
				    n = n->parent;
                }

                if(n != t->max || t->max->value != 0xFFFFFFFF)
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
		    if(n->link[0] && !(n->link[0]->color & 2))
            {
			    fromLeft = FALSE;
			    n = n->link[0];
			    continue;
            }
            n->color |= 2;
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
    *value = cand;
    return TRUE;
}

    
/*
Traverses the full tree (inorder) in O(n) time, without using a list,
and invoking the callback for each node.
*/
void rb_inorder(rbt *t, void (*callback)(rbnode *n))
{
    rbnode *n = t->min;
    BOOL fromLeft = FALSE;

    while(n)
    {
	    if (fromLeft || (!n->link[0] && ! (n->color & 2)))
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
                return;
            }
        }
	    if(!(n->color & 2))
        {
		    if(n->link[0] && !(n->link[0]->color & 2))
            {
			    fromLeft = FALSE;
			    n = n->link[0];
			    continue;
            }
            n->color |= 2;
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


#ifdef DEBUG_PRINT
void print_node(rbnode *n)
{
    pman_print_dbg("%i ", n->value);
}

void test_marks(rbnode *n)
{
    if(!n) return;
    if(n->color & 2)
        pman_print_dbg("\nmarked node: %i", n->value);
    if(n->link[0])
        test_marks(n->link[0]);
    if(n->link[1])
        test_marks(n->link[1]);
}

int blacks = 0;
int mblacks = -1;

int validate_rb_n(rbnode *n)
{
    if(!n) return 1;
    
    // 2.Both children of every red node are black.
    if(n->color == RED &&
        ((n->link[0] && n->link[0]->color != BLACK) || (n->link[1] && n->link[1]->color != BLACK)))
    {
        pman_print_dbg("\nchild of red node %i is not black.", n->value);
        return 0;
    }
    // 3.Every simple path from a given node to any of its descendant leaves contains the same number of black nodes.
    if(n->color == BLACK)
        blacks++;
    
    if(!n->link[0] && !n->link[1])
    {
        if(mblacks == -1)
            mblacks = blacks;
        else if(mblacks != blacks)
        {
            // it's a leaf, check blacks against the maximum blacks
            pman_print_dbg("\nfrom root to leaf %i there are different blacks count.", n->value);
            return 0;
        }
    }
    else
    {
        if(!validate_rb_n(n->link[0])) return 0;
        if(!validate_rb_n(n->link[1])) return 0;
    }
    if(n->color == BLACK)
        blacks--;
    return 1;
}

int validate_rb(rbt *t)
{
    mblacks = -1;
    blacks = 1;
    // 1.The root is black
    if(t->root->color != BLACK)
    {
        pman_print_dbg("\nroot is not black");
        return 0;
    }
    return validate_rb_n(t->root);
}
#endif
