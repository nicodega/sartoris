/*
*	Process and Memory Manager Service
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

#include <os/layout.h>
#include "pmanager_internals.h"
#include "page_list.h"

extern page_list free_pages;
extern int running;

struct taken_entries *taken;

void pm_claim_address_space(void *page_dir) 
{
    unsigned int **base, *table, entry;
    int table_idx, entry_idx;
    
    /* Nico: We HAVE to map to the linear address space in order to be able
       to read the tables */
    base = (unsigned int **)PHYSICAL2LINEAR(PG_ADDRESS(page_dir));

    /* NOTE: There are kernel pages mapped on the directory
       if page address is not within our limits, we will ignore it 
    */
    for (table_idx=0; table_idx<1024; table_idx++) 
	{
        table = base[table_idx];
	
        if ((unsigned int)table > FIRST_PAGE(PMAN_POOL_PHYS) && PG_PRESENT(table)) {

            table = (unsigned int *)PHYSICAL2LINEAR(PG_ADDRESS(table));

            for (entry_idx=0; entry_idx<1024; entry_idx++) 
			{
                entry = table[entry_idx];

                if (PG_ADDRESS(entry) >= FIRST_PAGE(PMAN_POOL_PHYS) && PG_PRESENT(entry)) 
				{
                     pm_put_page((void *)PG_ADDRESS(entry)); 
                }
            }
            pm_put_page((void*)LINEAR2PHYSICAL(PG_ADDRESS(table)));
        }
    }

    pm_put_page((void*)LINEAR2PHYSICAL(base));

}

void *pm_get_page()
{
	/* Get a free page from the pool */
	unsigned int i = 0, *addr = (unsigned int*)get_page(&free_pages);
	
	while(i < 0x400){ addr[i++] = 0;}

	/* Tell the page alocation system a page has been taken from the pool. */
	pa_page_taken();

	return (void*)LINEAR2PHYSICAL(addr);
}

void pm_assign(void *pgaddr_phys, unsigned int pman_tbl_entry, struct taken_entry *tentry)
{
	unsigned int pmlinear = PHYSICAL2LINEAR(pgaddr_phys);

	if(pman_tbl_entry != PMAN_TBL_ENTRY_NULL && !(TAKEN_PDIR(tentry) || TAKEN_PTBL(tentry)))
	{
		/* Page out the page, and modify our page table entry */
		page_out(PMAN_TASK, pmlinear + SARTORIS_PROCBASE_LINEAR, 2);

		/* Now set the page table entry */
		unsigned int *tbl = (unsigned int*)PM_TBL(pmlinear);

		tbl[PM_LINEAR_TO_TAB(pmlinear + SARTORIS_PROCBASE_LINEAR)] = pman_tbl_entry;
	}

	/* Now set the entry */
	set_taken(tentry, (void*)pgaddr_phys);
}

void set_taken(struct taken_entry *tentry, void *phys_addr)
{
	unsigned int pmlinear = PHYSICAL2LINEAR(phys_addr);

	struct taken_table *ttbl = taken->tables[PM_LINEAR_TO_DIR(pmlinear)];

	SET_TAKEN(tentry);

	ttbl->entries[PM_LINEAR_TO_TAB(pmlinear)] = *tentry;
}


struct taken_entry *get_taken(void *phys_addr)
{
	unsigned int pmlinear = PHYSICAL2LINEAR(phys_addr);

	struct taken_table *ttbl = taken->tables[PM_LINEAR_TO_DIR(pmlinear)];

	return (struct taken_entry *)&ttbl->entries[PM_LINEAR_TO_TAB(pmlinear)];
}

/*
	Put a page.
*/
void pm_put_page(void *physical)
{
	unsigned int pmlinear = PHYSICAL2LINEAR(physical);

	struct taken_table *ttbl = taken->tables[PM_LINEAR_TO_DIR(pmlinear)];
	struct taken_entry *tentry = ((struct taken_entry*)&ttbl->entries[PM_LINEAR_TO_TAB(pmlinear)]);

	unsigned int *pmtbl = (unsigned int*)PM_TBL(pmlinear);

	/* If it was a lvl 2 page record, page in. */
	if(!PG_PRESENT(pmtbl[PM_LINEAR_TO_TAB(pmlinear + SARTORIS_PROCBASE_LINEAR)]) && IS_TAKEN(tentry) && !(TAKEN_PDIR(tentry) || TAKEN_PTBL(tentry)))
	{
		/* Page in page on pm address space */
		/* NOTE: I won't flush the TLB, because the page in our address space should 
		remain unused. And flush will occur on task switch. */
		page_in(PMAN_TASK, (void*)pmlinear + SARTORIS_PROCBASE_LINEAR, physical, 2, PGATT_WRITE_ENA);
	}

	/* Set page as not taken */
	SET_NOTTAKEN(tentry);

	/* Tell the page alocation system a page has been inserted on the pool. */
	pa_page_added();

	/* Get the task page table */
	put_page(&free_pages, (void*)pmlinear);	

}

void wake_pf_threads(int thread_id)
{
	/* Re-enable thread/s */
	thread_info[thread_id].fault_smo_id = -1;
	thread_info[thread_id].state = THR_WAITING;	
	thread_info[thread_id].flags &= ~THR_FLAG_PAGEFAULT;

	/* Go through task threads and enable all threads waiting for the same page.
	(pm_page_in is not necesary for the task is the same) */
	int curr_thr = thread_info[thread_id].fault_next_thread;
	int old_curr = thread_id;

	while(curr_thr != -1)
	{
		thread_info[curr_thr].fault_smo_id = -1;
		thread_info[curr_thr].state = THR_WAITING;	
		thread_info[curr_thr].flags &= ~THR_FLAG_PAGEFAULT;

		curr_thr = thread_info[curr_thr].fault_next_thread;
		thread_info[old_curr].fault_next_thread = -1;
		old_curr = curr_thr;
	}

	/* 
		If there are no threads on the swaptbl_next list
		unlock the table.
	*/

	/* Find predecesor on tbl list */		
	curr_thr = thread_info[thread_id].swaptbl_next;
	
	while(curr_thr != -1)
	{
		if(thread_info[curr_thr].swaptbl_next == thread_id) break;
		curr_thr = thread_info[curr_thr].swaptbl_next;
	}
	
	if(thread_info[thread_id].swaptbl_next == -1 && curr_thr == -1)
	{
		struct taken_entry *ptentry = get_taken((void*)thread_info[curr_thr].page_in_address);

		ptentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_IOLOCK;
	}
	else if(curr_thr != -1)
	{
		thread_info[curr_thr].swaptbl_next = thread_info[thread_id].swaptbl_next;
	}

	thread_info[thread_id].swaptbl_next = -1;
}

/*
This function will perform a page in with the initial age.
*/
int pm_page_in(int task, void *linear, void *physical, int level, int attrib)
{
	unsigned int **dir;
	unsigned int entry;

	if(page_in(task, linear, physical, level, attrib) == SUCCESS)
	{
		if(task == PMAN_TASK) return SUCCESS;

		/* Set age */	
		switch(level)
		{
			case 0:
				// do nothing for page directories
				break;
			case 1:
				/* page table */
				dir = (unsigned int**)PHYSICAL2LINEAR(task_info[task].page_dir);

				dir[PM_LINEAR_TO_DIR(linear)] = (unsigned int*)( ((unsigned int)PA_INITIAL_AGE << 9) | (unsigned int)dir[PM_LINEAR_TO_DIR(linear)] );

				break;
			case 2:
				/* page */
				dir = (unsigned int**)PHYSICAL2LINEAR(task_info[task].page_dir);

				unsigned int *tbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(dir[PM_LINEAR_TO_DIR(linear)]));

				tbl[PM_LINEAR_TO_TAB(linear)] = (PA_INITIAL_AGE << 9) | tbl[PM_LINEAR_TO_TAB(linear)];

				break;
		}

		return SUCCESS;
	}
	return FAILURE;
}
