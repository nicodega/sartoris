

#include "sartoris/cpu-arch.h"
#include "cpu-arch-inline.h"
#include "paging.h"
#include "lib/indexing.h"
#include "sartoris/kernel.h"

void *free_addr;
int int_count;

#ifdef PAGING

pd_entry dyn_tables[(MAX_ALLOC_LINEAR / 0x400000) - KERN_TABLES];

#define DYN_TBL_INDEX(a) (a - KERN_TABLES)

/* 
Flag used when we are on a sartoris 
dynamic memory page fault 
*/
int rq_int;
/* Page granted by OS */
void *rq_physical;

void arch_init_dynmem()
{
	int i;

	for(i = 0; i < ((MAX_ALLOC_LINEAR / 0x400000) - KERN_TABLES);i++)
		dyn_tables[i] = NULL;
}

/*
This will grant a page to sartoris. (Used by the OS when whe generate a sartoris PF).
*/
int arch_grant_page_mk(void *physical) 
{
	if(!rq_int) return FAILURE;

	rq_physical = physical;

    return SUCCESS;
}


/*
This function will map a kernel address if it page faulted
because page table had not been updated.
*/
int arch_kernel_pf(void *laddr)
{
	struct i386_task *tinf;
	pd_entry *pdir_map;
	
	pdir_map = AUX_PAGE_SLOT(curr_thread);
	tinf = GET_TASK_ARCH(curr_task);

	map_page(tinf->pdb);

	/* Check its a table, its created and its not present */
	if((pdir_map[PG_LINEAR_TO_DIR(laddr)] & PG_PRESENT) == 1 || dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))] == NULL)
		return FAILURE;

	pdir_map[PG_LINEAR_TO_DIR(laddr)] = dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))];

	return SUCCESS;
}


/*
This function must request a page from the operating system. It *must*
do so by issuing a Page Fault interrupt.
It must return a linear sartoris address already mapped.
NOTE: This function will break atomicity.
*/
int arch_request_page(void *laddr)
{
	struct i386_task *tinf_it, *tinf;
	pd_entry *pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);

	/* 
	We will request a page from underling OS by issuing a page fault 
	with thread/task = -1 and linear 0x0.
	Yeap, we will be trusting our OS on top... not nice... but it 
	provides dynamic memory until we figure out something better.
	*/

	/* 
	We have 200 MB at our disposal. We will need 50 page tables for mapping 
	them all. (substracting KERN_TABLES)
	Page tables will be allocated on demand for the MK, and returned when no 
	more entries are used (except for the mapping zone ones). This means
	we will have to update *every* existing task page directory, with new tables.
	If you think of something better let me know.
	*/

	/* Check if page table for this address is present on current task. */
	tinf = GET_TASK_ARCH(curr_task);

	map_page(tinf->pdb);

	if((pdir_map[PG_LINEAR_TO_DIR(laddr)] & PG_PRESENT) == 0)
	{
		if(dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))] == NULL)
		{
			rq_physical == NULL;

			/* Request a page for the table using a page fault */
			arch_issue_page_fault();

			if(rq_physical == NULL)
				return FAILURE;
			
			/*
			we will put the table only on current task. On other tasks 
			*/
			pdir_map[PG_LINEAR_TO_DIR(laddr)] = PG_ADDRESS(rq_physical) | PG_USER | PG_PRESENT | PGATT_WRITE_ENA;
			dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))] = PG_ADDRESS(rq_physical) | PG_USER | PG_PRESENT | PGATT_WRITE_ENA;
		}
		else
		{
			pdir_map[PG_LINEAR_TO_DIR(laddr)] = dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))];
		}
	}
	
	rq_physical = NULL;

	/* Request the page */
	arch_issue_page_fault();

	if(rq_physical == NULL)
			return FAILURE;

	map_page((pt_entry *)PG_ADDRESS(pdir_map[PG_LINEAR_TO_DIR(laddr)]));
	
	/* 
	Since directories share the same page table, we can
	safely change it only once.
	*/
	ptab_map[PG_LINEAR_TO_TAB(laddr)] = PG_ADDRESS(rq_physical) | PG_USER | PG_PRESENT | PGATT_WRITE_ENA;
	
	return SUCCESS;
}

/*
This function must return a page to the operating system. It *must*
do so by issuing a PF Interrupt.
If we could return the page to the OS, it must return 1, 0 otherwise.
*/
int arch_return_page(void *laddr)
{
	struct i386_task *tinf_it, *tinf;
	pd_entry *pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);
	int count = 0, i = 0;

	/* Remove page from table */
	map_page(tinf->pdb);

	if((pdir_map[PG_LINEAR_TO_DIR(laddr)] & PG_PRESENT))
	{
		/* Count present entries on table, and free it if necesary */
		map_page((pt_entry *)PG_ADDRESS(pdir_map[PG_LINEAR_TO_DIR(laddr)]));

		for(i = 0; i < 0x400; i++)
		{
			if(ptab_map[i] & PG_PRESENT)
				count++;
		}

		free_addr = (void*)PG_ADDRESS(ptab_map[PG_LINEAR_TO_TAB(laddr)]);
		ptab_map[PG_LINEAR_TO_TAB(laddr)] = 0;

		/* 
		If table has only one entry, we will return the table as well.
		Interrupt count will be 2. 
		*/
		if(count == 1) 
			int_count = 2;
		else
			int_count = 1;

		/* First page fault will free the page */
		arch_issue_page_fault();

		if(count == 1)
		{
			map_page(tinf->pdb); // Map directory again to access it
			
			free_addr = (void*)PG_ADDRESS(pdir_map[PG_LINEAR_TO_DIR(laddr)]);

			// remove page table from task directories
			tinf_it = first_task;
			while(tinf_it != NULL)
			{
				map_page(tinf->pdb);
				pdir_map[PG_LINEAR_TO_DIR(laddr)] = 0;
				tinf_it = tinf_it->next;
			}

			dyn_tables[DYN_TBL_INDEX(PG_LINEAR_TO_DIR(laddr))] = NULL;

			// issue page fault to free the table
			arch_issue_page_fault();
		}
	}

	return 1;
}

/*
Get physical address for the page being freed by arch_return_page.
*/
void *arch_get_freed_physical()
{	
	return free_addr;
}

/*
Returns how many interrupts will be issued on order to free the page.
*/
int arch_get_ret_count()
{
	return int_count;
}

#endif



