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

/*
Since memory area must support overlaping memory segments, and we will
use them to efficiently find a collition with other memory, I'll implement 
an Interval tree, using an augmented Red-Black tree.
This will allow us to insert, remove and query for collitions in O(log(n)) time.
*/

#include "vmm_memarea.h"


#define RED   1
#define BLACK 0

#define MAX(a,b) ((a > b)? a : b)
ma_node *ma_single_rotation(ma_node *n, int dir );
ma_node *ma_double_rotation(ma_node *n, int dir );
void ma_insert_rebalance(memareas *t, ma_node *n);

#define is_red(n) (n != NULL && n->color == RED)

static inline int intersects(ma_node *n, UINT32 low, UINT32 high)
{
    return (n->low < high && low < n->high);
}

/*
Find a colliding memory region.
This function runs in O(log(n)) time on the worst case.
*/
ma_node *ma_collition(memareas *t, UINT32 low, UINT32 high) 
{
	ma_node *current = *t;
			
	while (current != NULL && !intersects(current, low, high)) 
	{
		if(current->link[0] && current->link[0]->max > low)
			current = current->link[0];
		else
			current = current->link[1];
	}
		
	return current;
}

/*
Find all colliding memory regions.
This function runs in O(I + log(n)) time on the worst case,
where I is the ammount of intervals overlapping the provided
interval.
*/
void ma_overlaps(memareas *t, UINT32 low, UINT32 high, BOOL (*callback)(ma_node *n)) 
{
	ma_node *current = *t;
    UINT32 nlow = low;
    ma_node *lastn = NULL;

    do
    {
        lastn = NULL;
	    while (current != NULL) 
	    {
            // compare for intersection here. This way we can find a node
            // with a lower low value and a lower max value.
            if(intersects(current, nlow, high))
            {
                lastn = current;
            }

		    if(current->link[0] && current->link[0]->max > nlow)
			    current = current->link[0];
		    else if(current->link[1])
			    current = current->link[1];
	    }

        // raise the callback for the found node
        if(!callback(lastn)) return;

        // update nlow, setting nlow with lastn->max
        if(lastn) nlow = lastn->max;
        
    }while(lastn);
}

/*
Find a memory region by it's boundaries.
This function runs in O(log(n)) time on the worst case.
*/
ma_node *ma_search(memareas *t, UINT32 low, UINT32 high) 
{
	ma_node *current = *t;
			
	while (current != NULL && !(current->low == low && current->high == high)) 
	{
		if(low < current->low)
			current = current->link[0];
		else
			current = current->link[1];
	}
		
	return current;
}

/*
Return the first interval containing the specified memory position (p).
This function runs in O(log(n)) time on the worst case.
*/
ma_node *ma_search_point(memareas *t, UINT32 p) 
{
	ma_node *current = *t;
    	
    while (current != NULL && !(current->low <= p && current->high > p)) 
	{
		if(current->link[0] && current->link[0]->max > p)
        {
			current = current->link[0];
        }
		else
        {
			current = current->link[1];
        }
	}
    		
	return current;
}

ma_node *ma_search_low(memareas *t, UINT32 low) 
{
	ma_node *current = *t;
			
	while (current != NULL && current->low != low) 
	{
		if(current->low > low) 
			current = current->link[0];
		else
			current = current->link[1];
	}
		
	if(current != NULL)
        return current;
    else
        return NULL;
}

int ma_insert(memareas *t, ma_node *n) 
{
    ma_node *parent = NULL;
	ma_node *current = *t;
    
    n->link[0] = NULL;
    n->link[1] = NULL;
	n->max = n->high;	

    // traverse the tree and find a place for this new node
	while (current != NULL) 
	{
		parent = current;
			
		if (n->low < current->low) 
		{	
			current = current->link[0];
		}
		else
		{
            if(n->low == current->low)
               return FALSE;
            current = current->link[1];				
		}
        if(parent->max < n->high)
            parent->max = n->high;
	}

    // color the node red and hang it from the parent
	n->parent = parent;
	n->color = RED;

	if(parent == NULL) 
	{ 
		*t = n;
        (*t)->color = BLACK;
	} 
	else 
	{
		if (parent->low > n->low)
			parent->link[0] = n;
		else
			parent->link[1] = n;
        
        // fix the tree
	    ma_insert_rebalance(t, n);
	}
    
    if(n->link[0] && n->link[1])
        n->max = MAX(n->high, MAX(n->link[0]->max, n->link[1]->max));
    else if(n->link[0])
        n->max = MAX(n->high, n->link[0]->max);
    else if(n->link[1])
        n->max = MAX(n->high, n->link[1]->max);

    return TRUE;
}

// This algorithm implementation is a non-recursive adaptation of 
// an explanation on red black trees by Julienne Walker, thanks a lot!
// I thought the implementation was simple enough for all the 
// insertion cases and the code is cleaner and shorter this way.
void ma_insert_rebalance(memareas *t, ma_node *n) 
{
    ma_node *r = n->parent, *p = n;
    int dir;

    // the node n was inserted at the bottom
    // go up fixing the tree
    dir = r->low < n->low;

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
                r = ma_single_rotation( r, !dir );
            else if ( is_red( r->link[dir]->link[!dir] ) )
                r = ma_double_rotation( r, !dir );
        }
                
        p = r;
        r = r->parent;  // go to the upper tree
        if(r)
        {
            r->link[r->low < p->low] = p; // r might have changed, update it on it's parent
            dir = r->low < n->low;
        }
    }

    if(r == NULL)
        *t = p;
        
    // save us from a red violation at the root
	(*t)->color = BLACK;
}

// this function performs a rotation on the specified direction (0 left, 1 right)
ma_node *ma_single_rotation( ma_node *n, int dir )
{
    ma_node *save = n->link[!dir];

    n->link[!dir] = save->link[dir];
    if(n->link[!dir]) 
        n->link[!dir]->parent = n;
    save->link[dir] = n;
    save->parent = n->parent;
    n->parent = save;

    n->color = RED;
    save->color = BLACK;

    if(n->link[0] && n->link[1])
        n->max = MAX(n->high, MAX(n->link[0]->max, n->link[1]->max));
    else if(n->link[0])
        n->max = MAX(n->high, n->link[0]->max);
    else if(n->link[1])
        n->max = MAX(n->high, n->link[1]->max);

    if(save->link[0] && save->link[1])
        save->max = MAX(save->high, MAX(save->link[0]->max, save->link[1]->max));
    else if(save->link[0])
        save->max = MAX(save->high, save->link[0]->max);
    else if(save->link[1])
        save->max = MAX(save->high, save->link[1]->max);

    return save; // return the new root
}

// this function will perform first a rotation on the oposite direction
// on the node child on that direction and then on the specified
// direction on the node
ma_node *ma_double_rotation(ma_node *n, int dir )
{
    n->link[!dir] = ma_single_rotation(n->link[!dir], !dir );
    if(n->link[!dir])
        n->link[!dir]->parent = n;
    return ma_single_rotation(n, dir );
}

// this implements a top-down Red Black t deletion.
void ma_remove(memareas *t, ma_node *n)
{
    // This algorithm implementation was taken (mostly)
    // from an explanation on red black trees by Julienne Walker
    // thanks for that! deletion is really a pain in the ass!
    ma_node head = {0};
    ma_node *q, *p, *g, *s; /* Helpers */
    ma_node *f = NULL;      /* Found item */
    int dir = 1, last;
 
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
            last = dir;
 
            /* Update helpers */
            g = p, p = q;
            q = q->link[dir];
            dir = q->low < n->low;
 
            /* Save found node */
            if ( q->low == n->low )
                f = q;
 
            /* Push the red node down */
            if ( !is_red(q) && !is_red(q->link[dir]) ) 
            {
                if ( is_red(q->link[!dir]) )
                {
                    p = p->link[last] = ma_single_rotation(q, dir );
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
                                g->link[dir2] = ma_double_rotation(p, last );
                            else if ( is_red(s->link[!last]))
                                g->link[dir2] = ma_single_rotation(p, last );
 
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

            // fix maximum
            if(q->link[0] && q->link[1])
                q->max = MAX(q->high, MAX(q->link[0]->max, q->link[1]->max));
            else if(q->link[0])
                q->max = MAX(q->high, q->link[0]->max);
            else if(q->link[1])
                q->max = MAX(q->high, q->link[1]->max);
        }
 
        /* Update root and make it black */
        *t = head.link[1];
        if ( *t != NULL )
            (*t)->color = BLACK;
    }
}

