
#include "sartoris/kernel.h"
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
int dyn_pg_nest;		// Dynamic memory nesting. 0 = none, 1 = already alocated waiting to run thread, 2 = waiting for alocation.
int dyn_pg_thread;		// Thread where dynamic memory request originated
int dyn_pg_ret;			// If 0 no page is being returned to the OS.
void *dyn_pg_ret_addr;  // Address returned from page fault handler on memory request

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
	dyn_pg_thread = 0;
	dyn_pg_ret = 0;
	dyn_pg_ret_addr = NULL;
	dyn_free_first = NULL;
	f_alloc = 0;
	f_count = DYN_GRACE_PERIOD;

	for(i=0; i < DYN_BITMAPSIZE; i++)
		pg_bitmap[i] = 0;
}

void dyn_free_queued();
/*
Get a page from OS (be careful on outher functions... this breaks atomicity)
This will return linear address of the page we got from OS.
NOTE: We count on atomicity here, until we invoke arch_request_page().
*/
void *dyn_alloc_page(int lvl)
{
	void *laddr = NULL;		// this is the address where we mapped the granted page
	
	/*
	Check if there is a non returned free page
	*/
	if(dyn_free_first != NULL)
	{
		laddr = dyn_free_first;

		dyn_free_first = dyn_free_first->next;

		f_alloc--;
		f_count = DYN_GRACE_PERIOD; // reset grace period

		return laddr;
	}

	/*
	Fail if we are already serving a dynamic memory need.
	*/
	if(dyn_pg_lvl != DYN_PGLVL_NONE) return NULL;

	/*
	Find a free page on sartoris linear using the bitmap
	*/
	int i,j;
	for(i=0; i < DYN_BITMAPSIZE; i++)
	{
		if((pg_bitmap[i] & 0xFFFFFFFF) != 0xFFFFFFFF)
		{
			/* we found a free page on this dword */
			for(j = 31; j >= 0; j--)
			{
				if((pg_bitmap[i] & (0x1 << j)) == 0)
				{
					laddr = (void*)(KERN_LMEM_SIZE + (i * 32 + (32 - j)) * PG_SIZE);
					break;
				}
			}
		}
	}

	if(i == DYN_BITMAPSIZE) return NULL;
	
	dyn_pg_lvl = lvl;			// indicate we are on a dynamic memory PF

	int ret = arch_request_page(laddr);
	
	dyn_pg_lvl = DYN_PGLVL_NONE;
	
	if(ret == FAILURE) return NULL;

	setbit(pg_bitmap, (unsigned int)(((unsigned int)laddr - KERN_LMEM_SIZE) / PG_SIZE), 0);

	return laddr;
}

void dyn_bmp_free(unsigned int laddr)
{
	laddr -= KERN_LMEM_SIZE;
	laddr = laddr / PG_SIZE;
	setbit(pg_bitmap, (unsigned int)(((unsigned int)laddr - KERN_LMEM_SIZE) / PG_SIZE),0);
}

/*
Free a page used by sartoris.
This function asumes we are on an atomic block.
This will break atomicity.
*/
void dyn_free_page(void *linear, int lvl)
{
	/* Attempt to return the page */
	struct dyn_free_page *pg = NULL;
	int ret;
		
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
			
	/* Check if some other thread is busy freeing. */
	if(dyn_pg_ret != 0)
	{
		f_alloc++;
		return;
	}
		
	dyn_pg_ret++;				// this value will be checked on interrupt.c

	if(f_alloc == DYN_THRESHOLD)
	{
		f_count = DYN_GRACE_PERIOD;
	}
	
	f_alloc++;
	
	if(f_alloc > DYN_THRESHOLD)
	{
		if(f_count == 0)
		{	
			dyn_free_queued(DYN_THRESHOLD / 2);		
		}
		else
		{
			f_count--;
		}
	}
	
	dyn_pg_ret = 0; 
}

void dyn_free_queued(int count)
{
	struct dyn_free_page *pg;
	int ret = 1;

	while(dyn_free_first != NULL && ret && count > 0)
	{
		/* 
		Remove the page from the list (just in case some other free is performed
			and it finished while we broke atomicity)
		*/
		pg = dyn_free_first;
		dyn_free_first = pg->next;

		dyn_pg_ret_addr = pg;
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
			/* Set page as free on bitmap */
			dyn_bmp_free((unsigned int)pg);

			f_count--;
		}
		count--;
	}	
}


