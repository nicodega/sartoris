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


void calc_mem(struct multiboot_info *mbinf);

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
			if(mbe->start <= phy && mbe->end > phy && mbe->type != 1)
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
  	page_count = vmm.vmm_size / 0x1000;

	map_pages(PMAN_TASK
				, (UINT32)PMAN_POOL_LINEAR + (UINT32)SARTORIS_PROCBASE_LINEAR
				, (UINT32)PMAN_POOL_PHYS
				, page_count
				, PGATT_WRITE_ENA
				, 2);
	
	vmm.available_mem = 0;

	/* Load the low pages stack */
	lo_page_count = (PHYSICAL2LINEAR(0x1000000) - FIRST_PAGE(PMAN_POOL_LINEAR)) / 0x1000;	// first 16MB - (sartoris + pman + pman tables)
	
	init_page_stack(&vmm.low_pstack);

    /* We must be careful here not to put services image pages on the pool 
    (they'll be added later once they are initialized) */
	
    /* Low mem stack will be loaded bottom-up */
    ignore_s = PHYSICAL2LINEAR(ignore_start);
    ignore_e = PHYSICAL2LINEAR(ignore_end);
	ignored = 0;
	
	for(offset = lo_page_count; offset > 0; offset--) 
	{
      	 add = FIRST_PAGE(PMAN_POOL_LINEAR) + (offset-1) * 0x1000;
		 
		 // Check we are not inserting the init physical pages 
		 if((add < ignore_s || add >= ignore_e) && avaliablePage(multiboot, (ADDR)(FIRST_PAGE(PMAN_POOL_PHYS) + (offset-1) * 0x1000)))
         {
		 	push_page(&vmm.low_pstack, (ADDR)add);
		    vmm.available_mem += 0x1000; 
         }
		 else
         {
		 	ignored++;
         }		 
    }

	pman_print("VMM: Loaded low stack, total: %i, ignored: %i, pushed: %i ", lo_page_count, ignored, lo_page_count - ignored);

	/* Now load high memory pages */
	page_count = vmm.vmm_size / 0x1000 - vmm.vmm_tables - lo_page_count;

    init_page_stack(&vmm.pstack);
    
	ignored = 0;

    /* We must be careful here not to put services image pages on the pool */	     
    for(offset = 0; offset < page_count; offset++) 
	{
      	 add = PHYSICAL2LINEAR(0x1000000) + offset * 0x1000;

		 // Check we are not inserting the init physical pages 
		 if((add < ignore_s || add >= ignore_e) && avaliablePage(multiboot, (ADDR)(PHYSICAL2LINEAR(0x1000000) + offset * 0x1000)))
         {
		 	push_page(&vmm.pstack, (ADDR) (add));
		    vmm.available_mem += 0x1000;
         }
		 else
         {
		 	ignored++;
         }
    }

	pman_print("VMM: Loaded High Stack, total: %i, ignored: %i, pushed: %i ", page_count, ignored, page_count - ignored);
    /* Stacks have been set up, take pages for our taken structure */
	
	/* 
		First tables (200MB + PMAN + MALLOC + POOL TABLES) will be taken by pman 
        and pool page tables, and should never be used/accessed on the structure.
		They'll remain to be 0.
	*/
	page_count = vmm.vmm_tables;

	for(i = 0; i < page_count; i++)
	{
		vmm.taken.tables[i] = NULL;
	}
	
    dentry = PM_LINEAR_TO_DIR(FIRST_PAGE(PMAN_POOL_LINEAR));

	for(i = 0; i <= page_count; i++) 
	{
		k = 0;
		addr = (UINT32*)pop_page(&vmm.pstack);
	
		for(k = 0; k < 0x400; k++){ addr[k] = 0;}
        vmm.available_mem -= 0x1000;
		
		vmm.taken.tables[dentry + i] = (struct taken_table*)addr;
    }

	pman_print("VMM: Taken Directory allocated. pages: %i, size: %x ", page_count, page_count * 0x1000);
	
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
    
	/* Initialize memory regions. */
	vmm.region_descriptors.first = NULL;
	vmm.region_descriptors.total = 0;

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
				push_page(&vmm.low_pstack, (ADDR)cstart);
		        vmm.available_mem += 0x1000;
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
	      		push_page(&vmm.pstack, (ADDR)cstart);
		        vmm.available_mem += 0x1000;
            }
		}
	}
	return 1;
}

struct page_stack *vmm_addr_stack(ADDR pman_laddress)
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
		vmm.pysical_mem = (mbinf->mem_upper + 0x1000) * 0x1000;	// mem upper is memory above the MB and is in kb
	}

	/* Attempt to improve memory limits */
	mmap_mem_size = 0;

	if((mbinf->flags & MB_INFO_MMAP) && mbinf->mmap_length > 0)
	{
		mbe = (struct mmap_entry*)mbinf->mmap_addr;
		size = 0;

		while(size != mbinf->mmap_length)
		{
			if(mbe->end > mmap_mem_size)
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
	vmm.vmm_tables = POOL_MEGABYTES / 4;        // tables needed for the addressing all memory on the system

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
	ADDR page = pop_page(((low_mem)? &vmm.low_pstack : &vmm.pstack));

	if(page == NULL) return NULL;

	/* Set the taken structure */
	tentry = vmm_taken_get(page);

	/* Set taken by PMAN */
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
	push_page(vmm_addr_stack(page_laddr), page_laddr);

	/* Set the taken structure */
	tentry = vmm_taken_get(page_laddr);

	/* Set taken by PMAN */
	tentry->data.b = 0;

	vmm.available_mem++;
}

// Get a page for a process page directory
ADDR vmm_get_dirpage(UINT16 task_id)
{
	struct taken_table *ttable = NULL;
	struct taken_entry *tentry = NULL;

	/* Get a page from High Stack */
	ADDR page = pop_page(&vmm.pstack);

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
	ADDR page = pop_page(&vmm.pstack);

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
	ADDR page = pop_page(((low_mem)? &vmm.low_pstack : &vmm.pstack));

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
	push_page(vmm_addr_stack(page_laddress), page_laddress);
	
	/* Set the taken structure */
	tentry = vmm_taken_get(page_laddress);

	/* If it was a common page, page it in */
	if(!tentry->data.b_pg.dir && !tentry->data.b_pg.tbl)
	{
		page_in(PMAN_TASK, (ADDR)((UINT32)page_laddress + SARTORIS_PROCBASE_LINEAR), (ADDR)LINEAR2PHYSICAL(page_laddress), 2, PGATT_WRITE_ENA);
	}

	tentry->data.b = 0;	

	vmm.available_mem++;
}
// claim a task address space completely
void vmm_claim(UINT16 task)
{
	struct vmm_page_directory *dir = NULL;
	struct vmm_page_table *table = NULL;
	struct pm_task *tsk = tsk_get(task);
	UINT32 tbladdr, pgaddr, table_idx, entry_idx;
	
	if(tsk == NULL) return;

	/* This function will return a process tables to the stacks */
	dir = tsk->vmm_info.page_directory;
	table = NULL;
    tsk->vmm_info.page_directory = NULL;
    
    /* NOTE: There are kernel pages mapped on the directory
       if page address is not within our limits, we will ignore it 
    */
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
					vmm_put_page((ADDR)PHYSICAL2LINEAR(pgaddr));
                }
            }
            vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbladdr)));
        }
    }

	vmm_put_page((ADDR)PG_ADDRESS(tsk->vmm_info.page_directory));
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
return SUCCESS; // REMOVE THIS
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

			tsk->io_finished.callback = cmd_task_destroyed_callback;
			io_begin_close( &tsk->io_event_src );
		}
		return FALSE;
	}
	return TRUE;
}

void vmm_close_task(struct pm_task *task)
{
	/* Remove Shared and Physically mapped regions */
	struct vmm_memory_region *mreg = task->vmm_info.regions.first;
	struct vmm_memory_region *next = NULL; 

	while(mreg != NULL)
	{
		next = mreg->next;

		if(mreg->type != VMM_MEMREGION_FMAP)
		{
			struct vmm_region_descriptor *des = (struct vmm_region_descriptor*)mreg->descriptor;

			des->references--;

			if(des->references == 0)
			{
				/* Free region */
				if(mreg->type == VMM_MEMREGION_MMAP)
				{
					vmm_phy_umap(task, mreg->tsk_lstart);
				}
				else
				{
					vmm_share_remove(task, mreg);
				}
			}
		}
		mreg = next;
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
	vmm_info->regions.first = NULL;
	vmm_info->regions.total = 0;
    vmm_info->wait_root = NULL;
    vmm_info->tbl_wait_root = NULL;
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

