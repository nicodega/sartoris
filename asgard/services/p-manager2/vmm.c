/*
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


#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "layout.h"
#include "formats/bootinfo.h"
#include "formats/ia32paging.h"
#include "formats/ia32paging.h"
#include "vm.h"
#include "mem_layout.h"
#include "helpers.h"
#include "taken.h"
#include "task_thread.h"
#include <services/pmanager/services.h>
#include "rb.h"

void calc_mem(struct multiboot_info *mbinf);
phy_allocator *vmm_addr_stack(ADDR pman_laddress);

/* 
Returns TRUE if this page can be used. False if BIOS has assigned for mapped IO.
*/
BOOL avaliablePage(struct multiboot_info *multiboot, ADDR physical)
{
	UINT32 size = 0;
	struct mmap_entry *mbe = NULL;
	UINT64 phy = (0x00000000FFFFFFFF & ((UINT32)physical));

	/* If there is a memory map present, go through it */
	if((multiboot->flags & MB_INFO_MMAP) && multiboot->mmap_length > 0)
	{
		mbe = (struct mmap_entry*)multiboot->mmap_addr;
		size = 0;

		while(size != multiboot->mmap_length)
		{
			if(mbe->start <= phy && mbe->end > phy && mbe->type != MULTIBOOT_MMAP_AVAILABLE)
				return FALSE;
		
			size += mbe->size;
			mbe = (struct mmap_entry*)( (UINT32)mbe + mbe->size);
		}
	}
	return TRUE;
}

INT32 vmm_init(struct multiboot_info *multiboot, UINT32 ignore_start, UINT32 ignore_end)
{
	UINT32 page_count, lo_page_count, ignore_s, ignore_e, add;
	UINT32 dentry, k, *addr, start, offset, i, ignored;

    ignore_s = PHYSICAL2LINEAR(ignore_start);
    ignore_e = PHYSICAL2LINEAR(ignore_end);
	ignored = 0;

	pman_print("Calculating memory...");

	calc_mem(multiboot);

	vmm.multiboot = multiboot;
	
	pman_print_set_color(10);
	pman_print("VMM: ignore start %x, end %x ", ignore_start, ignore_end);
	pman_print("VMM: linear start: %x, size: %i MB, tables: %i , phy start: %x ", vmm.vmm_start, vmm.pool_MB, vmm.vmm_tables, (UINT32)PMAN_POOL_PHYS);
	
	/* 
		We need page tables for the process manager, so we can access vmm memory 
		lets take the first pages from pool space for all tables
	*/
	map_pages(PMAN_TASK
				, (UINT32)PMAN_POOL_LINEAR + (UINT32)SARTORIS_PROCBASE_LINEAR
				, (UINT32)PMAN_POOL_PHYS
				, vmm.vmm_tables
				, PGATT_WRITE_ENA
				, 1);

	/* Now pagein whatever is left on the pool into the page tables. 
    (Here we also map our own page tables, thats why I dont do: vmm.vmm_size / 0x1000 - vmm.vmm_tables) */
  	page_count = (vmm.vmm_size >> 12);

	map_pages(PMAN_TASK
				, (UINT32)PMAN_POOL_LINEAR + (UINT32)SARTORIS_PROCBASE_LINEAR
				, (UINT32)PMAN_POOL_PHYS
				, page_count
				, PGATT_WRITE_ENA
				, 2);
	
	vmm.available_mem = 0;

    /*
    Get physical pages for taken structure.
    */
    UINT32 taken_start = page_count-1;
    UINT32 taken_got_a16 = 0, taken_got_b16 = 0, taken_got = 0;
    dentry = PM_LINEAR_TO_DIR(FIRST_PAGE(PMAN_POOL_LINEAR));

    page_count = vmm.vmm_tables;

	for(i = 0; i < page_count; i++)
	{
		vmm.taken.tables[i] = NULL;
	}

    i = 0;

    while(taken_got != vmm.vmm_tables)
    {
        add = PMAN_POOL_LINEAR + (taken_start << 12);
        
        if((add < ignore_s || add >= ignore_e) && avaliablePage(multiboot, (ADDR)(PMAN_POOL_PHYS + (taken_start << 12))))
        {            
            k = 0;
		    addr = (UINT32*)add;
	
            // all pages will initially be taken
		    for(k = 0; k < 0x400; k++){ addr[k] = 1;}

            vmm.taken.tables[dentry + i] = (struct taken_table*)addr;
            
            if(add < 0x1000000)
                taken_got_b16++;
            else
                taken_got_a16++;
            taken_got++;

            i++;
        }
        else
        {
            ignored++;
        }
        taken_start--;
    }
	    
	pman_print("VMM: Taken Directory allocated. pages: %i(b16) - %i(a16), size: %x ", taken_got_b16, taken_got_a16, page_count * 0x1000);

    /* Initialize physical memory allocator */
    pya_init(&vmm.low_pstack);
    pya_init(&vmm.pstack);

	/* Load the low pages stack */
	lo_page_count = ((PHYSICAL2LINEAR(0x1000000) - FIRST_PAGE(PMAN_POOL_LINEAR)) >> 12) - taken_got_b16;	// first 16MB - (sartoris + pman + pman tables)
		
    /* We must be careful here not to put services image pages on the pool 
    (they'll be added later once they are initialized) */
	
    /* Low mem stack will be loaded bottom-up */
    for(offset = lo_page_count; offset > 0; offset--) 
	{
      	 add = FIRST_PAGE(PMAN_POOL_LINEAR) + ((offset-1) << 12);
		 
		 // Check we are not inserting the init physical pages 
		 if((add < ignore_s || add >= ignore_e) && avaliablePage(multiboot, (ADDR)(FIRST_PAGE(PMAN_POOL_PHYS) + ((offset-1) << 12))))
            pya_put_page(&vmm.low_pstack, (ADDR)add, PHY_NONE);
         else
         	ignored++;
    }

	pman_print("VMM: Loaded low stack, total: %i, ignored: %i, pushed: %i ", lo_page_count, ignored, lo_page_count - ignored);

	/* Now load high memory pages */
	page_count = (vmm.vmm_size >> 12) - vmm.vmm_tables - (lo_page_count + taken_got_b16) - taken_got_a16;
        
	ignored = 0;

    /* We must be careful here not to put services image pages on the pool */	     
    for(offset = 0; offset < page_count; offset++) 
	{
      	 add = PHYSICAL2LINEAR(0x1000000) + (offset << 12);

		 // Check we are not inserting the init physical pages 
		 if((add < ignore_s || add >= ignore_e) && avaliablePage(multiboot, (ADDR)(PHYSICAL2LINEAR(0x1000000) + (offset << 12))))
             pya_put_page(&vmm.pstack, (ADDR)add, PHY_NONE);
         else
             ignored++;
    }

	pman_print("VMM: Loaded High Stack, total: %i, ignored: %i, pushed: %i ", page_count, ignored, page_count - ignored);
    /* Stacks have been set up, take pages for our taken structure */
	
	/* 
		First tables (SARTORIS MB + PMAN + MALLOC + POOL TABLES) will be taken by pman 
        and pool page tables, and should never be used/accessed on the structure.
		They'll remain to be 0.
	*/
	
    start = PMAN_POOL_LINEAR;
    i = PM_LINEAR_TO_DIR(PMAN_POOL_LINEAR + (UINT32)SARTORIS_PROCBASE_LINEAR);
	k = vmm.vmm_tables + i;

	/* Get pointers for our page tables. */
	for(; i < k; i++) 
	{
		vmm.assigned_dir.table[i] = (struct vmm_pman_assigned_table*)start;
		start += 0x1000;
	}
    
	vmm_swap_init();
    
    /* Initialize region trees */
    rb_init(&vmm.fmap_descriptors);
    vmm.phy_mem_areas = NULL;
    /* Initialize libraries list */
    vmm.slibs = NULL;
        
    /* Add protected memory areas */
    UINT32 size = 0, pgs;
	struct mmap_entry *mbe = NULL;
	UINT32 phy;
	
	pman_print("VMM: Init Finished");

	return 1;
}

/*
Adds a memory range to the vmm page stacks.
*/
INT32 vmm_add_mem(struct multiboot_info *multiboot, UINT32 start, UINT32 end)
{    
	/* Add pages to the page stack */

	/* Add Low mem stack pages */
	UINT32 cstart, cend;

	if(start < 0x1000000)
	{
		cend = MIN(0x1000000, end);
		for(cstart = 0x1000000 - start; cstart < cend; cstart += PAGE_SIZE) 
		{      
			if(avaliablePage(multiboot, (ADDR)LINEAR2PHYSICAL(cstart))) 
            {
                pya_put_page(&vmm.low_pstack, (ADDR)cstart, PHY_NONE);
            }
		}
	}

	/* Add High stack pages */
	if(end > 0x1000000)
	{
		cstart = MAX(0x1000000, start);
		for(; cstart < end; cstart += PAGE_SIZE) 
		{      
			if(avaliablePage(multiboot, (ADDR)LINEAR2PHYSICAL(cstart)))
            {
                pya_put_page(&vmm.pstack, (ADDR)cstart, PHY_NONE);
            }
		}
	}
        
	return 1;
}

phy_allocator *vmm_addr_stack(ADDR pman_laddress)
{
	if((UINT32)pman_laddress < 0x1000000)
		return &vmm.low_pstack;
	return &vmm.pstack;
}

void calc_mem(struct multiboot_info *mbinf)
{
	UINT32 size, mmap_mem_size;
	struct mmap_entry *mbe;
	
	vmm.pysical_mem = PMAN_DEFAULT_MEM_SIZE;

	// check mbinf flags
	if(mbinf->flags & MB_INFO_MEM)
	{
		vmm.pysical_mem = ((mbinf->mem_upper + 0x1000) << 12);	// mem upper is memory above the MB and is in kb
	}

	/* Attempt to improve memory limits */
	mmap_mem_size = 0;

	if((mbinf->flags & MB_INFO_MMAP) && mbinf->mmap_length > 0)
	{
		mbe = (struct mmap_entry*)mbinf->mmap_addr;
		size = 0;

		while(size != mbinf->mmap_length)
		{
			if(mbe->end > mmap_mem_size
				&& mbe->type != MULTIBOOT_MMAP_RESERVED && !(mbe->type >= MULTIBOOT_MMAP_BAD_MEMORY))
				mmap_mem_size = mbe->end;
			size += mbe->size;
			mbe =  (struct mmap_entry *)((UINT32)mbe + mbe->size);	
		}
	}

	if(mmap_mem_size != 0)
	{
		vmm.pysical_mem = mmap_mem_size;
	}
	
    // now lets calculate pool megabytes
    vmm.pool_MB = (vmm.pysical_mem - PMAN_POOL_PHYS) / 0x100000;

    if(vmm.pool_MB % 4 != 0)
        vmm.pool_MB -= (vmm.pool_MB % 4);

    vmm.vmm_start = (ADDR)PMAN_POOL_LINEAR;
	vmm.vmm_size = POOL_MEGABYTES * 0x100000;	// size of the pool in bytes
	vmm.vmm_tables = POOL_MEGABYTES / 4;        // tables needed for addressing all memory on the system

}

struct taken_entry *vmm_taken_get(ADDR laddress)
{
	struct taken_table *ttable = vmm.taken.tables[PM_LINEAR_TO_DIR(laddress)];
	if(ttable == NULL)
		pman_print_and_stop("NULL TAKEN TABLE");
	return &ttable->entries[PM_LINEAR_TO_TAB(laddress)];
}

// Get a page for process manager internal use
// returns a linear address
ADDR vmm_pm_get_page(BOOL low_mem)
{
	struct taken_entry *tentry = NULL;

	/* Get a page from stack */
	ADDR page = pya_get_page(((low_mem)? &vmm.low_pstack : &vmm.pstack), FALSE);

	if(page == NULL) return NULL;

	/* Set the taken structure */
	tentry = vmm_taken_get(page);

	/* Set taken by PMAN */
    tentry->data.b = 0;
	tentry->data.b_pg.flags = TAKEN_PG_FLAG_PMAN;
	tentry->data.b_pg.taken = 1;

	vmm.available_mem--;
	return page;
}

// Return a page to the free set
void vmm_pm_put_page(ADDR page_laddr)
{
	struct taken_entry *tentry = NULL;

	if(page_laddr == NULL) return;

	/* Return the page to the Stack */
    pya_put_page(vmm_addr_stack(page_laddr), (ADDR)page_laddr, PHY_NONE);
    
	vmm.available_mem++;
}

// Get a page for a process page directory
ADDR vmm_get_dirpage(UINT16 task_id)
{
	struct taken_table *ttable = NULL;
	struct taken_entry *tentry = NULL;

	/* Get a page from High Stack */
	ADDR page = pya_get_page(&vmm.pstack, FALSE);

	if(page == NULL) return NULL;

	/* Set the taken structure */
	ttable = vmm.taken.tables[PM_LINEAR_TO_DIR(page)];
	tentry = &ttable->entries[PM_LINEAR_TO_TAB(page)];

	/* Set taken for directory */
	tentry->data.b = 0;
	tentry->data.b_pdir.dir = 1;
	tentry->data.b_pdir.taskid = (UINT16)task_id;
	tentry->data.b_pdir.taken = 1;

	vmm.available_mem--;
	return page;
}

// Get a page for a process page table
ADDR vmm_get_tblpage(UINT16 task_id, UINT32 proc_laddress)
{
	struct taken_entry *tentry = NULL;

	/* Get a page from High Stack */
	ADDR page = pya_get_page(&vmm.pstack, FALSE);

	if(page == NULL) return NULL;

	/* Set the taken structure */
	tentry = vmm_taken_get(page);

	/* Set taken for directory */
	tentry->data.b = 0;
	tentry->data.b_ptbl.tbl = 1;
	tentry->data.b_ptbl.taken = 1;
	tentry->data.b_ptbl.taskid = task_id;
	tentry->data.b_ptbl.dir_index = PM_LINEAR_TO_DIR(proc_laddress);

	vmm.available_mem--;

	return page;
}

/*
This function will return a page, but won't remove it from PMAN
address space. This should only be used for pages which cannot be 
sent to swap, or invoke vmm_unmap_page after this function.
*/
ADDR vmm_get_page(UINT16 task_id, UINT32 proc_laddress)
{
	return vmm_get_page_ex(task_id, proc_laddress, FALSE);
}
ADDR vmm_get_page_ex(UINT16 task_id, UINT32 proc_laddress, BOOL low_mem)
{
	struct pm_task *tsk = NULL;
	struct taken_entry *tentry = NULL;

	/* Get a page */
	ADDR page = pya_get_page(((low_mem)? &vmm.low_pstack : &vmm.pstack), FALSE);
    
	if(page == NULL)
    {
        pman_print_dbg("vmm_get_page_ex: NULL Page \n");
        return NULL;
    }

	tsk = tsk_get(task_id);

    if(tsk == NULL)
    {
        pman_print_dbg("vmm_get_page_ex: NULL Task \n");
        return NULL;
    }

	/* Set the taken structure */
	tentry = vmm_taken_get(page);

	if(tentry->data.b_pg.taken == 1)
		pman_print_and_stop("VMM: GRANTING ALREADY TAKEN PAGE");

	/* Set taken for page */
	tentry->data.b = 0;
	tentry->data.b_pg.taken = 1;
	tentry->data.b_pg.tbl_index = PM_LINEAR_TO_TAB(proc_laddress);
	tentry->data.b_pg.eflags = ((tsk->flags & TSK_FLAG_SERVICE) || (tsk->flags & TSK_FLAG_SYS_SERVICE))? TAKEN_EFLAG_SERVICE : TAKEN_EFLAG_NONE;
	
	vmm.available_mem--;

    unsigned char *pg = (unsigned char *)page;
    int k =0;
    // REMOVE
    for(k = 0; k < 0x1000; k++)
    {
        *pg = 0;
        pg++;
    }

	return page;
}

/* 
Unmap a page from pman linear space.
Used for common pages (not directories nor tables).
*/
void vmm_unmap_page(UINT16 task_id, UINT32 proc_laddress)
{
	struct vmm_pman_assigned_record *assigned = NULL;
    
    ADDR page = (ADDR)(PHYSICAL2LINEAR(vmm_get_physical(task_id, (ADDR)proc_laddress)) + (UINT32)SARTORIS_PROCBASE_LINEAR);
        
	/* 
	Un-Map the page from pman linear space, for it's table entry might be used
	for swapping 
	*/
	page_out(PMAN_TASK, (ADDR)page, 2);

	/*
	Set PMAN assigned record as not free.
	*/
	assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(page)]->entries[PM_LINEAR_TO_TAB(page)];
	
	assigned->dir_index = PM_LINEAR_TO_DIR(proc_laddress);
	assigned->flags = 0;
	assigned->not_assigned = 0;	// set page as assigned
	assigned->task_id = task_id;	
}

UINT32 vmm_temp_pgmap(struct pm_task *task, ADDR proc_laddress)
{
    struct vmm_pman_assigned_record *assigned = NULL;
    struct vmm_pman_assigned_record assigned_bck;
    UINT32 paddr = (UINT32)vmm_get_physical(task->id, proc_laddress);
    UINT32 pm_laddr = PHYSICAL2LINEAR(paddr) + (UINT32)SARTORIS_PROCBASE_LINEAR;

    assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(pm_laddr)]->entries[PM_LINEAR_TO_TAB(pm_laddr)];
    assigned_bck = *assigned;
    page_in(PMAN_TASK, (ADDR)pm_laddr, (ADDR)paddr, 2, PGATT_WRITE_ENA);

    return *((UINT32*)&assigned_bck);
}

void vmm_restore_temp_pgmap(struct pm_task *task, ADDR proc_laddress, UINT32 assigned_bck)
{
    struct vmm_pman_assigned_record *assigned = NULL;
    UINT32 pm_laddr = PHYSICAL2LINEAR(vmm_get_physical(task->id, proc_laddress)) + (UINT32)SARTORIS_PROCBASE_LINEAR;

    page_out(PMAN_TASK, (ADDR)pm_laddr, 2);

    assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(pm_laddr)]->entries[PM_LINEAR_TO_TAB(pm_laddr)];
    *assigned = *((struct vmm_pman_assigned_record*)&assigned_bck);
}

struct vmm_pman_assigned_record *vmm_get_assigned(UINT16 task_id, UINT32 proc_laddress)
{
	ADDR page = (ADDR)(PHYSICAL2LINEAR(vmm_get_physical(task_id, (ADDR)proc_laddress)) + (UINT32)SARTORIS_PROCBASE_LINEAR);

	return &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(page)]->entries[PM_LINEAR_TO_TAB(page)];
}

/* Get a physical address based on a task linear address */
ADDR vmm_get_physical(UINT16 task, ADDR laddress)
{
	struct pm_task *tsk = tsk_get(task);

    if(tsk == NULL) return NULL;

	/* We have to get the physical address for this linear address */
	struct vmm_page_directory *dir = tsk->vmm_info.page_directory;
	struct vmm_page_table *tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(dir->tables[PM_LINEAR_TO_DIR(laddress)].b));

	return (ADDR)PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddress)].entry.phy_page_addr);
}

/* 
Get a physical table address based on a task linear address.
*/
struct vmm_page_table *vmm_get_tbl_physical(UINT16 task, ADDR laddress)
{
	struct pm_task *tsk = tsk_get(task);

    if(tsk == NULL) return NULL;

	struct vmm_page_directory *dir = tsk->vmm_info.page_directory;
	return (struct vmm_page_table*)PG_ADDRESS(dir->tables[PM_LINEAR_TO_DIR(laddress)].b);
}

void vmm_set_flags(UINT16 task, ADDR laddress, BOOL eflag, UINT32 flag, BOOL enabled)
{
	struct pm_task *tsk = tsk_get(task);

    if(tsk == NULL) return;

	/* Set the taken structure */
	struct taken_entry *tentry = vmm_taken_get(laddress);
    
	if(eflag)
	{
		if(enabled)
			tentry->data.b_pdir.eflags = (tentry->data.b_pdir.eflags | flag);
		else
			tentry->data.b_pdir.eflags = (tentry->data.b_pdir.eflags & ~flag);
	}
	else
	{		
		if(enabled)
			tentry->data.b_pg.flags = (tentry->data.b_pg.flags | flag);
		else
			tentry->data.b_pg.flags = (tentry->data.b_pg.flags & ~flag);
	}
}


/*
Return a page to the stack
NOTE: If page was swapped, it should be swapped out before calling this function
*/
void vmm_put_page(ADDR page_laddress)
{
	struct taken_entry *tentry = NULL;

	if(page_laddress == NULL) return;

	if((UINT32)page_laddress < FIRST_PAGE(PMAN_POOL_LINEAR))
		pman_print_and_stop("PMAN: Assert 1");
    	
    /* Return the page to the Stack */
    pya_put_page(vmm_addr_stack(page_laddress), (ADDR)page_laddress, PHY_REMAP);
    
	vmm.available_mem++;
}

void vmm_put_pages(ADDR page_laddress, int pages, int io, int free_io)
{
	struct taken_entry *tentry = NULL;
    int flags = PHY_REMAP;

	if(page_laddress == NULL) return;

	if((UINT32)page_laddress < FIRST_PAGE(PMAN_POOL_LINEAR))
		pman_print_and_stop("PMAN: Assert 1");

    if(io)
        flags |= PHY_IO;
    if(free_io)
        flags |= PHY_FREE_IO;

	pya_put_pages(vmm_addr_stack(page_laddress), page_laddress, pages, flags);
}

// claim a task address space completely
// NOTE: this function should be invoked after vmm_close_task
// has finished.
void vmm_claim(struct pm_task *task)
{
	struct vmm_page_directory *dir = NULL;
	struct vmm_page_table *table = NULL;
	UINT32 tbladdr, pgaddr, table_idx, entry_idx, addr;
	
	if(task == NULL) return;

	/* This function will return a process tables to the stacks */
	dir = task->vmm_info.page_directory;
	table = NULL;
    task->vmm_info.page_directory = NULL;
    
    /* NOTE: There are kernel pages mapped on the directory
       if page address is not within our limits, we will ignore it 
    */
    addr = SARTORIS_PROCBASE_LINEAR;

    for(table_idx = PM_LINEAR_TO_DIR(SARTORIS_PROCBASE_LINEAR); table_idx < 1024; table_idx++) 
	{
		tbladdr = PG_ADDRESS(dir->tables[table_idx].b);
	
		if((UINT32)tbladdr > FIRST_PAGE(PMAN_POOL_PHYS) && dir->tables[table_idx].ia32entry.present) 
		{
            table = (struct vmm_page_table*)PHYSICAL2LINEAR(tbladdr);

            for(entry_idx = 0; entry_idx < 1024; entry_idx++) 
			{
			    pgaddr = PG_ADDRESS(table->pages[entry_idx].entry.phy_page_addr);
                
				if(pgaddr >= FIRST_PAGE(PMAN_POOL_PHYS) && table->pages[entry_idx].entry.ia32entry.present == 1) 
				{
                    // if we are above PMAN_MAPPING_BASE we might have
                    // to skip vmm_put_page.
                    // NOTE: libraries will be loaded from SARTORIS_PROCBASE_LINEAR
                    // on it's task so this won't affect them
                    if((addr < (PMAN_MAPPING_BASE + SARTORIS_PROCBASE_LINEAR)) || 
                        (addr >= (PMAN_MAPPING_BASE + SARTORIS_PROCBASE_LINEAR) && (vmm_taken_get((ADDR)PHYSICAL2LINEAR(pgaddr))->data.b_pg.flags & TAKEN_PG_FLAG_LIBEXE) == 0))
                    {
                        vmm_put_page((ADDR)PHYSICAL2LINEAR(pgaddr));
                    }
                }
                addr += PAGE_SIZE;
            }
            vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbladdr)));
        }
        else
        {
            addr += 0x400000;   // skip the table
        }
    }

	vmm_put_page((ADDR)PG_ADDRESS(task->vmm_info.page_directory));
}


// claim a task memory region space.
void vmm_claim_region(struct pm_task *task, struct vmm_memory_region *mreg)
{
	struct vmm_page_directory *dir = NULL;
	struct vmm_page_table *table = NULL;
	UINT32 tbladdr, pgaddr, table_idx, entry_idx, addr, max;
    int entries;
	
	if(task == NULL) return;

	/* This function will return a process tables to the stacks */
	dir = task->vmm_info.page_directory;
	table = NULL;
    
    addr = mreg->tsk_node.low;
    max = mreg->tsk_node.high;

    for(table_idx = PM_LINEAR_TO_DIR(addr); table_idx < 1024 && addr < max; table_idx++) 
	{
		tbladdr = PG_ADDRESS(dir->tables[table_idx].b);
	
		if(dir->tables[table_idx].ia32entry.present) 
		{
            table = (struct vmm_page_table*)PHYSICAL2LINEAR(tbladdr);
            
            entries = 0;

            for(entry_idx = 0; entry_idx < 1024; entry_idx++) 
			{
			    pgaddr = PG_ADDRESS(table->pages[entry_idx].entry.phy_page_addr);
                
                if(table->pages[entry_idx].entry.ia32entry.present == 1)
                {
                    if(addr < max)
                    {
				        if((vmm_taken_get((ADDR)PHYSICAL2LINEAR(pgaddr))->data.b_pg.flags & TAKEN_PG_FLAG_LIBEXE) == 0)
                        {
				            page_out(task->id, (ADDR)addr, 2);
                            vmm_put_page((ADDR)PHYSICAL2LINEAR(pgaddr));
                        }
                        else
                        {   
                            page_out(task->id, (ADDR)addr, 2);
                        }
                        task->vmm_info.page_count--;
                    }
                    else
                    {
                        entries++;
                    }
                }
                addr += PAGE_SIZE;
            }

            if(entries == 0)
            {
                page_out(task->id, (ADDR)TBL_ADDRESS(addr), 1);
                vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbladdr)));
                task->vmm_info.page_count--;
            }
        }
        else
        {
            addr += 0x400000;   // skip the table
        }
    }
}

/*
This function will perform a page-in with the initial age.
*/
INT32 pm_page_in(UINT16 task_id, ADDR linear, ADDR physical, UINT32 level, INT32 attrib)
{
	struct vmm_page_directory *dir = NULL;
	struct vmm_page_table *tbl = NULL;
	struct pm_task *task = tsk_get(task_id);

    if(task == NULL)
    {
        pman_print_dbg("pm_page_in: NULL TASK %i \n", task_id);
        return NULL;
    }
	
	if((UINT32)physical <= 0x100000)
		pman_print_and_stop("Attempt to page in phy: %x, task: %i, linear: %x ", physical, task_id, linear);

	if(page_in(task->id, linear, physical, level, attrib) == SUCCESS)
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
				dir = task->vmm_info.page_directory;
				dir->tables[PM_LINEAR_TO_DIR(linear)].b = ((UINT32)VMM_INITIAL_AGE << 9) | dir->tables[PM_LINEAR_TO_DIR(linear)].b;
				break;
			case 2:
				/* page */
				dir = task->vmm_info.page_directory;
				tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(dir->tables[PM_LINEAR_TO_DIR(linear)].b));

				tbl->pages[PM_LINEAR_TO_TAB(linear)].entry.phy_page_addr = (VMM_INITIAL_AGE << 9) | tbl->pages[PM_LINEAR_TO_TAB(linear)].entry.phy_page_addr;

				break;
		}

		return SUCCESS;
	}
	else
	{
		pman_print_dbg("page_in failed %x \n", linear);
	}
	return FAILURE;
}

/*
Returns true if available memory (physical + swap) is enough 
to hold the whole task loadable segments.
*/
BOOL vmm_can_load(struct pm_task *tsk)
{
	/* Available pages will be calculated by using SWAP available pages + Available Pool Memory */
	UINT32 available_pages = vmm.swap_available + vmm.available_mem;
	
	if(available_pages < tsk->vmm_info.expected_working_set)
	{
		/* Begin close */
		if(tsk != NULL && tsk->state != TSK_NOTHING) 
		{
			tsk->state = TSK_KILLING;
			tsk->command_inf.command_sender_id = 0;
			tsk->command_inf.command_req_id = -1;
			tsk->command_inf.command_ret_port = -1;

			tsk->command_inf.ret_value = PM_NOT_ENOUGH_MEM;

			tsk->io_finished.callback = cmd_task_fileclosed_callback;
			io_begin_close( &tsk->io_event_src );
		}
		return FALSE;
	}
	return TRUE;
}

void vmm_close_task(struct pm_task *task)
{
	/* Remove Shared and Physically mapped regions */
	struct vmm_memory_region *mreg;
	struct vmm_memory_region *next = NULL; 

    while(task->vmm_info.regions)
    {
        // NOTE: each function invoked will remove the node from the tree.
        mreg = VMM_MEMREG_MEMA2MEMR(task->vmm_info.regions);

        struct vmm_region_descriptor *des = (struct vmm_region_descriptor*)mreg->descriptor;

		/* Free region */
        if(mreg->type == VMM_MEMREGION_FMAP)
		{
            // this will be processed async
            if(vmm_fmap_task_closing(task, mreg) == 2)
                return;
		}
		else if(mreg->type == VMM_MEMREGION_MMAP)
		{
			vmm_phy_umap(task, (ADDR)mreg->tsk_node.low, 0);
		}
        else if(mreg->type == VMM_MEMREGION_LIB)
		{
			vmm_lib_unload(task, mreg);
		}
		else
		{
			vmm_share_remove(task, mreg->tsk_node.low);
		}
    }
            
    // if there are threads from other task waiting for a page
    // from this task, we will have to wake them when the task is destroyed

	/* Begin swap empty procedure. */
	vmm_swap_empty(task, FALSE);
}

void vmm_init_task_info(struct task_vmm_info *vmm_info)
{
	vmm_info->page_directory = NULL;
	vmm_info->swap_free_addr = (ADDR)0xFFFFFFFF;
	vmm_info->table_swap_addr = 0;
	vmm_info->page_count = 0;
	vmm_info->swap_page_count = 0;
	vmm_info->expected_working_set = 0;
	vmm_info->max_addr = 0;
	vmm_info->swap_read_smo = -1;
    rb_init(&vmm_info->regions_id);
    vmm_info->regions = NULL;
    rb_init(&vmm_info->wait_root);
    rb_init(&vmm_info->tbl_wait_root);
}

void vmm_init_thread_info(struct pm_thread *thread)
{
    struct thread_vmm_info *vmm_info = &thread->vmm_info;
	vmm_info->fault_address = NULL;
	vmm_info->page_in_address = NULL;
	vmm_info->page_displacement = 0;
	vmm_info->read_size = 0;
	vmm_info->page_perms = 0;
	vmm_info->fault_smo = -1;
	vmm_info->fault_region = NULL;
    vmm_info->pg_node.value2 = thread->id;
    vmm_info->tbl_node.value2 = thread->id;
    vmm_info->pg_node.next = NULL;
    vmm_info->tbl_node.next = NULL;
    vmm_info->pg_node.prev = NULL;
    vmm_info->tbl_node.prev = NULL;
}
