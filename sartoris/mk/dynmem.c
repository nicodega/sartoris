
#include "sartoris/kernel.h"
#include "sartoris/kernel-data.h"

/*
Initialize dynamic memory subsystem.
*/
void dyn_init()
{
#ifdef _DYNAMIC_
	dyn_pg_lvl = DYN_PGLVL_NONE;
	phys_page = NULL;
#endif
}

#ifdef _DYNAMIC_

int dyn_pg_lvl;

/*
Get a page from OS (be careful on outher functions... this breaks atomicity)
This will return linear address of the page we got from OS.
*/
void *dyn_alloc_page(int lvl)
{
	void *laddr = NULL;		// this is the address where we mapped the granted page
	
	/*
	Fail if we are already serving a dynamic memory need.
	*/
	if(dyn_pg != DYN_PGLVL_NONE) 
		return NULL;
	/* 
	In order to allocate a page, we will rise a page fault interrupt
	*/
	dyn_pg = lvl;			// indicate we are on a dynamic memory PF

	// Do we need a page table for this page?
	
	// IMPLEMENT //

	/*
	Verify we where given a physical page
	*/
	if(phys_page == NULL) 
		return NULL;

	/*
	Map table
	*/

	// IMPLEMENT // 
	phys_page = NULL;

	// Get a page now

	// IMPLEMENT //

	/*
	 last_page_fault.task_id = -1;
	 last_page_fault.thread_id = -1;
	 last_page_fault.linear = 0x0;
       
	 arch_issue_page_fault();
	*/

	dyn_pg_lvl = DYN_PGLVL_NONE;

	/*
	Verify we where given a physical page
	*/
	if(phys_page == NULL) 
		return NULL;

	/*
	Map the page onto sartoris address space
	*/

	// IMPLEMENT //

	phys_page = NULL;

	return laddr;
}

/*
Free a page used by sartoris
*/
void dyn_free_page(void *linear, int lvl)
{
	/*
	Raise a non mascarable interrupt
	*/

	// IMPLEMENT //

}

#else

/*
No dynamic memory
*/
void *dyn_alloc_page(int lvl)
{
	return NULL;
}

void dyn_free_page(void *linear, int lvl){}

#endif // _DYNAMIC_

