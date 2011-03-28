
#include "sartoris/kernel.h"
#include "sartoris/metrics.h"
#include "sartoris/kernel-data.h"
#include "lib/bitops.h"
#include "lib/dynmem.h"
#include "sartoris/cpu-arch.h"
#include "kernel-arch.h"

#define DYN_PAGES ((MAX_ALLOC_LINEAR - KERN_LMEM_SIZE) / PG_SIZE)
#define DYN_BITMAPSIZE BITMAP_SIZE(DYN_PAGES)

/* 
If a page is not returned to the OS, it will be placed here.
When a dynamic memory need is issued again, the page will be returned
without request.
*/
struct dyn_free_page *dyn_free_first;
unsigned int f_alloc;	// free count
unsigned int f_count;   // grace period counter

/*
Each free page not returned to the OS will be placed on a linked list.
*/
struct dyn_free_page
{
	struct dyn_free_page *next;
	struct dyn_free_page *prev;
};

int dyn_pg_lvl;			// If DYN_PGLVL_NONE, no dynamic memory operations are being performed, otherwise
						// it will contain a value indicating which memory need is being served.
int dyn_pg_nest;		// Dynamic memory nesting. 0 = none, 1 = already allocated waiting to run thread, 2 = waiting for allocation.
						// When nesting is 0 we can allocate dynamic memory, if not, there's already a dynamic memory allocation going on.
int dyn_pg_thread;		// Thread where dynamic memory request originated
int dyn_pg_ret_thread;  // Thread returning pages to the OS.
int dyn_remaining;		// how many pages must the OS still provide.

/*
Bitmap used for memory allocation
*/
unsigned int pg_bitmap[DYN_BITMAPSIZE];
/*
Initialize dynamic memory subsystem.
*/
void dyn_init()
{
	int i = 0;

	dyn_pg_lvl = DYN_PGLVL_NONE;
	dyn_pg_nest = 0;
	dyn_pg_thread = -1;
	dyn_pg_ret_thread = -1;
	dyn_free_first = NULL;
	f_alloc = 0;
	f_count = DYN_GRACE_PERIOD;

	for(i=0; i < DYN_BITMAPSIZE; i++)
		pg_bitmap[i] = 0;
}

void dyn_free_queued(int);
/*
Get a page from OS (be careful on outher functions... this breaks atomicity)
This will return the linear address of the page we got from the underlying OS.
NOTE: We count on atomicity here, until we invoke arch_request_page().
*/
void *dyn_alloc_page(int lvl)
{
	void *laddr = NULL;		// this is the address where we mapped the granted page
	int i = 0, j = 0;

	/*
	Check if there is a non returned free page
	*/
	if(dyn_free_first != NULL)
	{
		laddr = dyn_free_first;

		dyn_free_first = dyn_free_first->next;

		f_alloc--;
		f_count = DYN_GRACE_PERIOD; // reset grace period
#ifdef METRICS
        metrics.alloc_dynamic_pages--;
        metrics.dynamic_pages++;
#endif
		return laddr;
	}

	/*
	Fail if we are already serving a dynamic memory need.
	*/
	if(dyn_pg_lvl != DYN_PGLVL_NONE) return NULL;

	/*
	Find a free page on sartoris linear space using the bitmap
	*/
	for(i=0; i < DYN_BITMAPSIZE; i++)
	{
		if((pg_bitmap[i] & 0xFFFFFFFF) != 0xFFFFFFFF)
		{
			/* we found a free page on this dword */
			for(j = 0; j < (sizeof(unsigned int) * 8); j++)
			{
				if((pg_bitmap[i] & (0x80000000 >> j)) == 0)
				{
					laddr = (void*)(KERN_LMEM_SIZE + (i * 32 + j) * PG_SIZE);
					break;
				}
			}
            if(laddr != NULL)
                break;
		}
	}

	if(i * 32 + j >= DYN_PAGES) return NULL;
	
    dyn_pg_thread = curr_thread; // we need this in order to generate the interrupt
	dyn_pg_lvl = lvl;	         // indicate we are on a dynamic memory PF
    
    bprintf(12, "mk/dynmem.c: ASKING FOR PAGE type: %i \n", lvl);
	    
    int ret = arch_request_page(laddr);
	
	dyn_pg_lvl = DYN_PGLVL_NONE;
    dyn_pg_thread = -1;
	
	if(ret == FAILURE) return NULL;

	setbit(pg_bitmap, (unsigned int)(((unsigned int)laddr - KERN_LMEM_SIZE) / PG_SIZE), 1);

#ifdef METRICS
    metrics.dynamic_pages++;
#endif
	return laddr;
}

/*
Free a page used by sartoris.
- This function asumes we are on an atomic block.
- This will break atomicity.
*/
void dyn_free_page(void *linear, int lvl)
{
	struct dyn_free_page *pg = NULL;
		
	/* Add page to non-returned pages list */
	pg = (struct dyn_free_page *)linear;

	if(dyn_free_first == NULL)
	{
		pg->next = NULL;
		pg->prev = NULL;
	}
	else
	{
		dyn_free_first->prev = pg;
		pg->next = dyn_free_first;
		pg->prev = NULL;
	}
	dyn_free_first = pg;
    f_alloc++;

#ifdef METRICS
    metrics.dynamic_pages--;
    metrics.alloc_dynamic_pages++;
#endif

	/* Check if some other thread is busy freeing. */
	if(dyn_pg_ret_thread != NULL)
	{
		return;
	}
		
	if(f_alloc > DYN_THRESHOLD)
	{
		if(f_count == 0)
        {
            dyn_pg_ret_thread = curr_thread;
			dyn_free_queued(DYN_THRESHOLD >> 1); // atomicity might be broken in here!
            dyn_pg_ret_thread = -1;
        }
		else
        {
			f_count--;
        }
	}
    else if(f_alloc == DYN_THRESHOLD)
	{
		f_count = DYN_GRACE_PERIOD;
	}
}

void dyn_free_queued(int count)
{
	struct dyn_free_page *pg;
    unsigned int laddr;
	int ret = 1;

	while(dyn_free_first != NULL && ret && count > 0)
	{
		/* 
		    Remove the page from the list (just in case some other free is performed
			and it finished while we broke atomicity)
		*/
		pg = dyn_free_first;
		dyn_free_first = pg->next;
        
		ret = arch_return_page(pg);

		/* Atomicity might have been broken here, be careful */
		if(!ret)
		{
			/* We must add our page back to the list */
			if(dyn_free_first == NULL)
			{
				pg->next = NULL;
				pg->prev = NULL;
			}
			else
			{
				dyn_free_first->prev = pg;
				pg->next = dyn_free_first;
				pg->prev = NULL;
			}

			dyn_free_first = pg;
		}
		else
		{
#ifdef METRICS
            metrics.alloc_dynamic_pages--;
#endif
			/* Set page as free on bitmap */
	        setbit(pg_bitmap, (unsigned int)(((unsigned int)laddr - KERN_LMEM_SIZE) / PG_SIZE), 0);

            f_alloc--;
		}
		count--;
	}	
}
