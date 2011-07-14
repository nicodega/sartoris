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

ma_node *ma_uncle(ma_node *n);
ma_node *ma_gparent(ma_node *n);
void ma_lrotation(memareas *t, ma_node *n);
void ma_rrotation(memareas *t, ma_node *n);
ma_node *ma_single_rotation(memareas *t, ma_node *n, int dir );
ma_node *ma_double_rotation(memareas *t, ma_node *n, int dir );
void ma_fix_insert(memareas *t, ma_node *n);

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
Find a colliding memory region.
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
		    else
			    current = current->link[1];
	    }

        // raise the callback for the found node
        if(!callback(lastn)) return;

        // update nlow, setting nlow with lastn->max
        nlow = lastn->max;
        
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
            {
               return FALSE;
            }
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
	} 
	else 
	{
		if (parent->low > n->low)
			parent->link[0] = n;
		else
			parent->link[1] = n;
	}
    
    if(n->link[0] && n->link[1])
        n->max = MAX(n->high, MAX(n->link[0]->max, n->link[1]->max));
    else if(n->link[0])
        n->max = MAX(n->high, n->link[0]->max);
    else if(n->link[1])
        n->max = MAX(n->high, n->link[1]->max);

    // fix the tree
	ma_fix_insert(t, n);
    return TRUE;
}

ma_node *ma_uncle(ma_node *n)
{
    if(n->parent == NULL || n->parent->parent == NULL) 
		return NULL;
	if(n->parent->parent->link[0] == n->parent)
		return n->parent->parent->link[1];
	else
		return n->parent->parent->link[0];
}

ma_node *ma_gparent(ma_node *n)
{
    if(n->parent == NULL || n->parent->parent == NULL) 
			return NULL;
	return n->parent->parent;
}

void ma_fix_insert(memareas *t, ma_node *n) 
{
	ma_node *y = NULL;
		
	while (n->parent != NULL && n->parent->color == RED) 
	{
		if (n->parent == ma_gparent(n)->link[0]) 
		{
			y = ma_uncle(n);
				
			if (y != NULL && y->color == RED) 
			{
				n->parent->color = BLACK;
				y->color = BLACK;

				n = ma_gparent(n);
				if(n != NULL)
					n->color = RED;
			} 
			else 
			{
				if (n == n->parent->link[1]) 
				{
					n = n->parent;
					ma_lrotation(t, n);
				}
				n->parent->color = BLACK;
				ma_gparent(n)->color = RED;
				ma_rrotation(t, ma_gparent(n));
			}
		}
		else 
		{
			y = ma_uncle(n);
				
			if (y != NULL && y->color == RED) 
			{
				n->parent->color = BLACK;
				y->color = BLACK;
				n = ma_gparent(n);
				if(n != NULL)
					n->color = RED;
			} 
			else 
			{
				if (n == n->parent->link[0]) 
				{
					n = n->parent;
					ma_rrotation(t, n);
				}
				n->parent->color = BLACK;
				ma_gparent(n)->color = RED;
				ma_lrotation(t, n->parent->parent);
			}
		}
	}

    // save us from a red violation at the root
	(*t)->color = BLACK;
}

void ma_fix_maxs(ma_node *n, ma_node *y)
{
    // fix maxs
    if(n->link[0] && n->link[1])
        n->max = MAX(n->high, MAX(n->link[0]->max, n->link[1]->max));
    else if(n->link[0])
        n->max = MAX(n->high, n->link[0]->max);
    else if(n->link[1])
        n->max = MAX(n->high, n->link[1]->max);

    if(y->link[0] && y->link[1])
        y->max = MAX(y->high, MAX(y->link[0]->max, y->link[1]->max));
    else if(y->link[0])
        y->max = MAX(y->high, y->link[0]->max);
    else if(y->link[1])
        y->max = MAX(y->high, y->link[1]->max);
}

void ma_lrotation(memareas *t, ma_node *n) 
{
	/* Left rotation maintining coloring */
	ma_node *y = n->link[1];
		
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

    ma_fix_maxs(n,y);
}
	
void ma_rrotation(memareas *t, ma_node *n) 
{
	/* Right rotation maintining coloring */
	ma_node *y = n->link[0];
		
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

    ma_fix_maxs(n,y);
}

ma_node *ma_single_rotation(memareas *t, ma_node *n, int dir )
{    
    ma_node *save = n->link[!dir];

    if(dir == 0)
        ma_lrotation(t, n);
    else
        ma_rrotation(t, n);

    n->color = RED;
    save->color = BLACK;

    return save;
}

ma_node *ma_double_rotation(memareas *t, ma_node *n, int dir )
{
    n->link[!dir] = ma_single_rotation(t, n->link[!dir], !dir );
    return ma_single_rotation(t, n, dir );
}

// this implements a top-down Red Black t deletion.
void ma_remove(memareas *t, ma_node *n)
{
    // This algorithm implementation was taken (mostly)
    // from an explanation on red black trees by Julienne Walker
    // thanks for that! deletion is really a pain in the ass!
    ma_node head;
    ma_node *q, *p, *g, *s; /* Helpers. p is the parent node. q is the current node. g is the grandparent node. s is the sibling. */
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
            dir = (q->low < n->low);
 
            /* if we found the node, save it */
            if ( q->low == n->low )
                f = q;
 
            /* Push the red node down */
            if ( q->color == BLACK && q->link[dir]->color == BLACK ) 
            {
                if ( q->link[!dir]->color == RED )
                {
                    // current node sibling is red, and the current node is black.
                    // perform single rotation on the direction of the sought node.
                    p = ma_single_rotation (t, q, dir );
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
                            int dir2 = (g->link[1] == p);
 
                            if ( s->link[last]->color == RED )
                                ma_double_rotation(t, p, last );
                            else if ( s->link[!last]->color == RED )
                                ma_single_rotation(t, p, last );
 
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
            f->low = q->low;
            f->high = q->high;
            f->parent = q->parent;
            f->max = q->max;
            p->link[p->link[1] == q] = q->link[q->link[0] == NULL];
        }
 
        /* Update root and make it black */
        *t = head.link[1];
        if ( *t != NULL )
            (*t)->color = BLACK;
    }
}

/* 
See if a segment with the specified length can be inserted on the tree
without collitions. 
It'll return the start address available for the segment.
This function will take O(n) time.
*/
/*
unsigned int ma_alloc(memareas *t, unsigned int start, unsigned int length)
{
    ma_node *n = t->min;
    BOOL fromLeft = 0;
    unsigned int high = n->high;

    if(start < n->low && n->low - start >= length) return min;

    while(n)
    {
	    if (fromLeft || !n->link[0])
		{
            if((start < t->low && t->low < high && t->low - high >= length) || n == t->max )
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
                if(n == t->max)
                    return t->max->low;
                else
                    return high;
            }
            if(high < n->high)
                high = n->high;
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
    return 0;
}
*/
