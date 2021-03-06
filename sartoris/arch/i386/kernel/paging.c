/*  
 *   Sartoris microkernel i386 paging implementation
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@gmail.com      
 */

#include "sartoris/kernel.h"
#include "sartoris/cpu-arch.h" 
#include "i386.h"
#include "kernel-arch.h"
#include "paging.h"
#include "lib/indexing.h"
#include "lib/containers.h"
#include "sartoris/critical-section.h"
#include "sartoris/syscall.h"

#include "sartoris/scr-print.h"

#ifdef PAGING

/* the kernel page table(s) for mapping, shared by everybody */
pt_entry kern_ptables[KERN_TABLES][0x400]  __align(PG_SIZE);

void init_paging()
{
	pt_entry *ptab_ptr;
	pd_entry *pdir_ptr;
	unsigned int linear;
	unsigned int physical = 0;
	int i;
	struct i386_task *tinf = GET_TASK_ARCH(INIT_TASK_NUM);
	
    /* Build first MB page table */

    /* Low memory (BIOS, etc) */
	for (i=0; i<160; i++) 
	{
		kern_ptables[0][i] = physical | PG_WRITABLE | PG_PRESENT;
		physical += PG_SIZE;
	}

	/* Video memory */
	for (; i<192; i++) 
	{
		kern_ptables[0][i] = physical | PG_CACHE_DIS | PG_WRITABLE | PG_PRESENT;
		physical += PG_SIZE;
	}

    /* VIDEO BIOS, BIOS Shadow, etc (000C0000 - 0x100000), and kernel (0x100000 to 0x200000) */
	for (;i<512; i++) 
	{
		kern_ptables[0][i] = physical | PG_WRITABLE | PG_PRESENT;
		physical += PG_SIZE;
	}

    for (;i<1024; i++) 
	{
		kern_ptables[0][i] = 0;
	}

    /* 
    Initialize the last kernel table, in case it overlaps with 
    the dynamic memory area.
    */ 

    for (i = 0; i < 1024; i++) 
	{
		kern_ptables[KERN_TABLES-1][i] = 0;
	}

	/* 
		We just built a kernel page table that self-maps megabytes 0-2 of memory.
		it will be shared by everyone. 
	
		We set up a page directory for the init-thread. it links to the
		kernel page table, and to the user page table that we will build next. */

	/* IMPORTANT: it is crafted this way because page_in does not
		work without paging enabled. */

	/* Init page directory will be first page after image */
	pdir_ptr = tinf->pdb = (void*)(INIT_OFFSET+INIT_SIZE);

	for(i = 0; i < KERN_TABLES; i++)
	{
		pdir_ptr[i] = ((unsigned int)kern_ptables[i]) | PG_WRITABLE | PG_PRESENT;
	}

	/* clear the rest of the page directory */
	// NOTE: Start at i where it was left.
	for (; i<1024; i++) 
	{
		pdir_ptr[i] = 0;
	}
    
	/* point to user page table located next to init image (first page is for dir) */
	pdir_ptr[PG_LINEAR_TO_DIR(USER_OFFSET)] = (INIT_OFFSET+INIT_SIZE+PG_SIZE) | PG_WRITABLE | PG_USER | PG_PRESENT;
    
	start_paging(tinf);

	/* build user page table */
	/* IMPORTANT: paging _must_ be enabled at this point, page_in doesn't
		do the right thing otherwise */

	linear = USER_OFFSET;
	physical = INIT_OFFSET;

	/* Last 64Kb will be mapped to bootinfo structure left by the loader */
	for (i=0; i < INIT_PAGES - BOOTINFO_PAGES; i++)
	{		
		if(page_in(INIT_TASK_NUM, (void*)linear, (void*)physical, 2, PGATT_WRITE_ENA) == FAILURE)
		{
			k_scr_print("Paging.c: Failed mapping Init image",0x7);
			for(;;);
		}
		linear += PG_SIZE;
		physical += PG_SIZE;
	}

	linear = BOOTINFO_MAP+USER_OFFSET;
	physical = BOOTINFO_PHYS;

	/* Map bootinfo */
	for (i=0; i < BOOTINFO_PAGES; i++) 
	{		
		if(page_in(INIT_TASK_NUM, (void*)linear, (void*)physical, 2, PGATT_WRITE_ENA) == FAILURE)
		{
			k_scr_print("Paging.c: Failed mapping BootInfo and MMAP",0x7);
			for(;;);
		}
		linear += PG_SIZE;
		physical += PG_SIZE;
	}
}

void start_paging(struct i386_task *tinf) 
{
	/* Dec. 10 2006: I'll enable 4MB pages support on CR4 */
#ifdef _PENTIUM_
	__asm__ __volatile__ ("movl %%eax, %%cr4\r\n"
						  /* Set PSE bit 4 of cr4 (when we load cr3 we will clear the TLBs) */
						  "or %%eax, $0x10"
		                  "movl %%eax, %%cr4" : : ); 
#endif

	__asm__ __volatile__ ("movl %0, %%cr3" : : "r" (tinf->pdb) : "cc" ); 
	__asm__ __volatile__ ("movl %%cr0, %%eax\n\t"
	                      "orl $0x80000000, %%eax\n\t" 
                          "movl %%eax, %%cr0" :  :  : "cc", "eax" );
}

void map_page(void *physical) 
{
    void *addr = AUX_PAGE_SLOT(curr_thread);
    kern_ptables[PG_LINEAR_TO_DIR(addr)][PG_LINEAR_TO_TAB(addr)] = PG_ADDRESS(physical) | PG_WRITABLE | PG_PRESENT;
    invalidate_tlb((void*)PG_LINEAR_TO_TAB(addr)); // invalidate this page
}

pt_entry save_map() 
{
    void *addr = AUX_PAGE_SLOT(curr_thread);
    return kern_ptables[PG_LINEAR_TO_DIR(addr)][PG_LINEAR_TO_TAB(addr)];
}

void restore_map(pt_entry map) 
{
    void *addr = AUX_PAGE_SLOT(curr_thread);
    kern_ptables[PG_LINEAR_TO_DIR(addr)][PG_LINEAR_TO_TAB(addr)] = map;
	invalidate_tlb((void*)PG_LINEAR_TO_TAB(addr)); // invalidate this page
}

/* IMPORTANT: notice that page directories/tables are mapped to
              a temporary linear address in order to be accessed.
	          hence, arch_page_in/out do not work with paging 
	          disabled, since the map will have no effect if
              there is no address translation */

int arch_page_in(int task, void *linear, void *physical, int level, int attrib) 
{
	pd_entry *pdir_ptr;
	pt_entry *ptab_ptr;
	pd_entry pdir_entry;
	pd_entry old_entry;
	pt_entry ptab_entry;
	pd_entry *pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);
	int i, result;

	result = FAILURE;
	
	/* be humble: serialize access to page table structure */
	pdir_ptr = GET_TASK_ARCH(task)->pdb;

	if (pdir_ptr != NULL || level == 0) 
	{ 
		/* no page directory needed for level=0 */
	    /* no access to microkernel memory! (but allow < 1MB) */
		if ( (unsigned int) physical < KRN_OFFSET || (unsigned int) physical >= KRN_OFFSET+KRN_SIZE) 
		{  
			if ( (unsigned int) linear >= USER_OFFSET || level == 0  ) 
			{
				/* no remapping of microkernel memory! */
				switch (level) 
				{
					case 2: 
					{
						/* insert 4kb page in page table (level 2 page in) */
						/* first we build the page table entry that must be inserted: */
						ptab_entry = PG_ADDRESS(physical) | PG_USER | PG_PRESENT;

						if (attrib & PGATT_CACHE_DIS) 
						{
							ptab_entry |= PG_CACHE_DIS;   /*       (optimizing)       */
						}
						
						if (attrib & PGATT_WRITE_ENA) 
						{
							ptab_entry |= PG_WRITABLE;
						}

						if (attrib & PGATT_WRITE_THR) 
						{
							ptab_entry |= PG_WRITE_THR;
						}
						
						/* now we must find the exact location in physical memory where
							ptab_entry should be located: */
						
						map_page(pdir_ptr);
						
						pdir_entry = pdir_map[PG_LINEAR_TO_DIR(linear)]; /* entry in the page */
																		 /* directory for     */
																		 /* (linear) address  */   
						
						if ( pdir_entry & PG_PRESENT ) 
						{
							result = SUCCESS; /* ok we're done for level 2 */
						
							/* page table for (linear) address */
							ptab_ptr = (pt_entry *) PG_ADDRESS(pdir_entry);  
						
							map_page(ptab_ptr);
						
							i = PG_LINEAR_TO_TAB(linear);
						
							old_entry = ptab_map[i];
						
							if (task == curr_task && (old_entry & PG_PRESENT)) 
							{
								/* FIXME: this shouldn't be necessary! */
								/* Let's make sure the motherfucking simulator empties its
								unclefucking virtual TLBs */
						    
								invalidate_tlb((void *) PG_ADDRESS(old_entry));
						    } 
						
							ptab_map[i] = ptab_entry;
						}
						
						break;
					}
					case 1: /* insert 4kb page table in page directory (level 1 page in) */
					{
						result = SUCCESS; /* this is it for level 1 */
						
						/* first clear the page table */
						ptab_ptr = (pt_entry *) PG_ADDRESS(physical);
						
						map_page(ptab_ptr);
						
						for (i=0; i<1024; i++) 
						{
							ptab_map[i] = 0;
						}
						
						/* now insert entry in the page directory */
						pdir_entry = PG_ADDRESS(physical) | PG_WRITABLE | PG_USER | PG_PRESENT;
						
						i = PG_LINEAR_TO_DIR(linear);
						
						map_page(pdir_ptr);
						
						old_entry = pdir_map[i];
						pdir_map[i] = pdir_entry;
						
						if (task == curr_task && (old_entry & PG_PRESENT)) 
						{
							/* FIXME: this shouldn't be necessary! */
							/* Let's make sure the motherfucking simulator empties its
							unclefucking virtual TLBs */
							arch_flush_tlb();
						}
						
						break;
					}
					case 0:
						/* insert 4kb page directory (level 0 page in) */
						if (task != curr_task) 
						{ 							
							result = SUCCESS; /* done with level 0 */
                            						
							/* insert static kernel page tables */
							pdir_ptr = (pd_entry *)PG_ADDRESS(physical);
													
							map_page(pdir_ptr);
													
							for (i=0; i<(KERN_TABLES); i++) 
							{	
								pdir_map[i] = ((unsigned int)kern_ptables[i]) | PG_WRITABLE | PG_PRESENT;
							}
													
							/* and clear the rest of the page directory */
							for (; i<1024; i++) 
								pdir_map[i] = 0;
							
							((struct i386_task *)GET_TASK_ARCH(task))->pdb = pdir_ptr;
						}
						break;
				} /* no more cases */
			}
		}
	}

	return result;
}

int arch_page_out(int task, void *linear, int level) 
{
	pd_entry *pdir_ptr;
	pt_entry *ptab_ptr;
	pd_entry pdir_entry;
	pd_entry old_entry;
	pt_entry ptab_entry;
	pd_entry *pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);
	int i, x, result;  

	result = FAILURE;

	x = mk_enter(); /* enter critical block */

	/* serialize access to page table structure */

	pdir_ptr = ((struct i386_task *)GET_TASK_ARCH(task))->pdb;  

	if (pdir_ptr != NULL || level == 0) 
	{
        if ( (unsigned int) linear >= USER_OFFSET || level == 0  ) 
		{
			/* no unmapping of microkernel memory! */
      
			switch(level) 
			{
				case 2:
					/* invalidate a 4kb page in page table (level 2 page out) */
	
					map_page(pdir_ptr);

					pdir_entry = pdir_map[PG_LINEAR_TO_DIR(linear)];  
					if ( pdir_entry & PG_PRESENT ) 
					{
						result = SUCCESS;
	  
						ptab_ptr = (pt_entry *) PG_ADDRESS(pdir_entry);
	  
						map_page(ptab_ptr);
	  
						i = PG_LINEAR_TO_TAB(linear);
	  
						old_entry = ptab_map[i]; 
						ptab_map[i] = 0;
	  
						if ( task == curr_task ) 
						{
							invalidate_tlb((void *) PG_ADDRESS(old_entry));
						}
					}
					break;
					/*invalidate a 4kb page table in page directory (level 1 page out) */
				case 1:
					result = SUCCESS;
	
					map_page(pdir_ptr);
	
					pdir_map[PG_LINEAR_TO_DIR(linear)] = 0;
	
					if (task == curr_task) 
					{
						arch_flush_tlb();
					}
					break;
				case 0:
					if (task != curr_task) 
					{
						result = SUCCESS;
						((struct i386_task *)GET_TASK_ARCH(task))->pdb = NULL;
					}
			} /* no more cases */
		}
	}  
  
	mk_leave(x); /* exit critical block */

	return result;
}

/* this adjusts our one-page window to other address spaces */
int import_page(int task, void *linear, int *rw) 
{
	void *linear_trans;
	int result;
    int w;

	result = FAILURE;

	linear_trans =  arch_translate(task, MAKE_KRN_SHARED_PTR(task, linear), &w);

    if(rw)
        *rw = w;

	if (linear_trans != NULL) 
	{
		result = SUCCESS;
		map_page(linear_trans);
	}

	return result;
}

/* this does linear->physical translation */

/* this should be called within a critical block,
   atomicity is required! */
void *arch_translate(int task, void *address, int *rw) 
{
	pt_entry *ptab_ptr;
	pt_entry **pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry pent_map;
	void *result = NULL;

	if (((struct i386_task *)GET_TASK_ARCH(INIT_TASK_NUM))->pdb != NULL) 
	{
		map_page((void*)((struct i386_task *)GET_TASK_ARCH(task))->pdb); 	// the one-page window has the task page directory
		ptab_ptr = pdir_map[PG_LINEAR_TO_DIR(address)];						// get the table pointer from current task dir

		if ((unsigned int) ptab_ptr & PG_PRESENT) 
		{
	    	map_page(ptab_ptr); 							// this discards the offset created by flags
			pent_map = ptab_map[PG_LINEAR_TO_TAB(address)];	// get the page entry from the task table
	    
			if ((unsigned int) pent_map & PG_PRESENT) 
			{
                *rw = ((pent_map & PG_WRITABLE) == PG_WRITABLE); 
				result = (void*)PG_ADDRESS(pent_map);		// return physical address for the page
			}
		}
	}
  
  return result;
}    

/* verifies the page for address is present.
This function will return:
- SUCCESS if page is present
- -1 if table is present but not the page
- -2 if the table is not present
*/
int verify_present(void *address, int write) 
{
	pt_entry saved_map;
	pt_entry *ptab_ptr;
	pt_entry ptab_entry;
	pt_entry **pdir_map = AUX_PAGE_SLOT(curr_thread);
	pt_entry *ptab_map = AUX_PAGE_SLOT(curr_thread);
	int result;

	result = FAILURE;

	saved_map = save_map();

	if (((struct i386_task *)GET_TASK_ARCH(curr_task))->pdb != NULL) 
	{
		map_page((void*)((struct i386_task *)GET_TASK_ARCH(curr_task))->pdb); 
		ptab_ptr = pdir_map[PG_LINEAR_TO_DIR(address)];

		if ((unsigned int) ptab_ptr & PG_PRESENT) 
		{
			map_page(ptab_ptr); /* this discards the offset created by flags */
			ptab_entry = ptab_map[PG_LINEAR_TO_TAB(address)];

			if ((unsigned int) ptab_entry & PG_PRESENT) 
			{
				if (!write || ptab_entry & PG_WRITABLE) 
				{
					result = SUCCESS;
				}
			}
		}
	}

	restore_map(saved_map);

	return result;
}    

#endif


