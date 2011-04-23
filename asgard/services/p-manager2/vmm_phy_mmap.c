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
#include "types.h"
#include "vm.h"
#include "taken.h"
#include "task_thread.h"
#include "kmalloc.h"

/*
This function will map a physical address range to the task address space.
*/
BOOL vmm_phy_mmap(struct pm_task *task, ADDR py_start, ADDR py_end, ADDR lstart, ADDR lend, BOOL exclusive)
{
	UINT32 pstart = PHYSICAL2LINEAR(py_start);
	UINT32 pend = PHYSICAL2LINEAR(py_end);
	UINT32 laddr = TRANSLATE_ADDR(lstart, UINT32);
	UINT32 pg_addr;
	struct taken_entry *tentry = NULL;
	struct vmm_phymap_descriptor *pmap = NULL, *gpmap = NULL;
	struct vmm_memory_region *mreg, *mregit;
	ADDR pg_tbl = NULL, pg = NULL;
	struct vmm_page_table *tbl = NULL;
	UINT32 maps = 0;
	struct pm_thread *thread = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	struct vmm_pman_assigned_record *assigned = NULL;

	/* Check address is not above max_addr */
	if(task->vmm_info.max_addr <= (UINT32)lend)
	{
		return FALSE;
	}

	if((UINT32)lstart % 0x1000 != 0 || ((UINT32)lend) % 0x1000 != 0)
		return FALSE;

	if((UINT32)py_start % 0x1000 != 0 || ((UINT32)py_end) % 0x1000 != 0)
		return FALSE;

	/* Check it's not too low. */
	if((UINT32)py_start < FIRST_PAGE(PMAN_POOL_PHYS) && (UINT32)py_start > 0x1000)
		return FALSE;
	
	lstart = TRANSLATE_ADDR(lstart, ADDR);
	lend = TRANSLATE_ADDR(lend, ADDR);

	/* Check a thread is not page faulting on the region */
	thread = task->first_thread;

	while(thread != NULL)
	{
		if((thread->flags & (THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL)) && PG_ADDRESS(thread->vmm_info.fault_address) >= (UINT32)laddr && PG_ADDRESS(thread->vmm_info.fault_address) < (UINT32)lend)
			return FALSE;
		thread = thread->next_thread;
	}

	/* 
	Check if required physical range is mappeable.
	
	It will be mappeable if:
		- It's not already assigned to a task.
		- It's assigned to a task, but its an mmaping in non exclusive mode.
	*/
	while(pstart < pend)
	{
		tentry = vmm_taken_get((ADDR)pstart);

		if(tentry->data.b_pg.taken == 1)
		{
			if(tentry->data.b_pg.dir == 1 || tentry->data.b_pg.tbl == 1 || (tentry->data.b_pg.flags & (TAKEN_PG_FLAG_FILE | TAKEN_PG_FLAG_PMAN)))
			{
				return FALSE;	// taken and cannot be mapped
			}

			if(tentry->data.b_pg.flags & TAKEN_PG_FLAG_PHYMAP)
			{
				/* Check physical mapping. Make sure it's not exclusive. */
				pmap = (struct vmm_phymap_descriptor*)vmm.region_descriptors.first;

				while(pmap != NULL)
				{
					if(pmap->type == VMM_MEMREGION_MMAP && (UINT32)pmap->py_start <= (UINT32)py_start && (UINT32)pmap->py_end >= (UINT32)py_end)
					{
						/* Already mapped on exclusive mode? */
						if(pmap->exclusive || exclusive)
							return FALSE;

						maps++;

						if(maps > 1) return FALSE;	// If there are more than 1 mappings fail

						gpmap = pmap;
						break;
					}
				}
			}
			else if(!(tentry->data.b_pg.flags & TAKEN_PG_FLAG_SHARED))
			{
				/* 
				It's being used by a task but not physically mapped or shared... Unmap it!! :@
				NOTE: If a thread was working on this page, it'll page fault wen runned again.
				*/
				assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(PHYSICAL2LINEAR(pstart + (UINT32)SARTORIS_PROCBASE_LINEAR))]->entries[PM_LINEAR_TO_TAB(PHYSICAL2LINEAR(pstart + (UINT32)SARTORIS_PROCBASE_LINEAR))];

                if(tsk_get(assigned->task_id) == NULL) return 0;

				pdir = tsk_get(assigned->task_id)->vmm_info.page_directory;
				ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[assigned->dir_index].b));

				/* Check page is not dirty. */
				if(ptbl->pages[tentry->data.b_pg.tbl_index].entry.ia32entry.dirty == 1)
					return FALSE;

				pg_addr = PG_ADDRESS(PHYSICAL2LINEAR(ptbl->pages[tentry->data.b_pg.tbl_index].entry.phy_page_addr));
				
				/* Page out from task */
				page_out(assigned->task_id, (ADDR)(assigned->dir_index * 0x400000 + tentry->data.b_pg.tbl_index * 0x1000), 2);

				/* Put page on VMM */
				vmm_put_page((ADDR)pg_addr);
			}
		}
		pstart += 0x1000;
	}

	/*
	TODO: I should check task won't attempt to map on an executable mapped area.
	*/
		
	/* Check task address space can be mapped. */
	while(laddr < (UINT32)lend)
	{
		/* If page table is not present check it's not swapped */
		if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(laddr)].ia32entry.present == 1)
		{
			tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(laddr)].b));

			if(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.ia32entry.present == 1)
			{
				/* Page is present, check it's not IOLOCKED, etc */
				tentry = vmm_taken_get((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.phy_page_addr)));

				if(tentry->data.b_pg.dir == 1 || tentry->data.b_pg.tbl == 1 || tentry->data.b_pg.eflags != 0 || (tentry->data.b_pg.flags & (TAKEN_PG_FLAG_FILE | TAKEN_PG_FLAG_PMAN)))
					return FALSE;	// taken and cannot be mapped
				
				/* Unmap table and return physical address to PMAN */
				vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.phy_page_addr)));

				page_out(task->id, (ADDR)laddr, 2);
			}
			else if(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.record.swapped == 1)
			{
				return FALSE;	// page is swapped
			}
		}
		else if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(laddr)].record.swapped == 1)
		{
			return FALSE;	// page table is swapped
		}
		laddr += 0x1000;
	}

	/*
	Check there is no other memory region overlapping.
	*/
	mregit = task->vmm_info.regions.first;

	while(mregit != NULL)
	{
		if( (lstart <= mregit->tsk_lstart && lend >= mregit->tsk_lstart) 
			|| (lstart <= mregit->tsk_lend && lend >= mregit->tsk_lstart) )
		{
			return FALSE;
		}

		mregit = mregit->next;
	}

	/* 
	Ok, memory range is available... great! lets have those physical pages!! 
	*/
	laddr = (UINT32)lstart;
	pstart = PHYSICAL2LINEAR(py_start);

	/* Remove address range from stacks. */
	remove_range(&vmm.low_pstack, (ADDR)pstart, (ADDR)pend);
	remove_range(&vmm.pstack, (ADDR)pstart, (ADDR)pend);

	/* Now Map physical pages onto the task address space */
	while(pstart < pend)
	{
		/* Check page table is present, if not give it one. */
		if(task->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		{
			pg_tbl = vmm_get_tblpage(task->id, (UINT32)lstart);

			page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_tbl), 1, PGATT_WRITE_ENA);

			task->vmm_info.page_count++;
		}

		/* Now map the page. */
		page_in(task->id, (ADDR)lstart, (ADDR)pstart, 2, PGATT_WRITE_ENA);

		/* Set taken. */
		tentry = vmm_taken_get((ADDR)pstart);

		tentry->data.b = 0;
		tentry->data.b_pg.taken = 1;
		tentry->data.b_pg.flags = TAKEN_PG_FLAG_PHYMAP;

		/* Unmap page from PMAN address space. */
		vmm_unmap_page(task->id, (UINT32)lstart);

		pstart += 0x1000;
		lstart = (ADDR)((UINT32)lstart + 0x1000);
	}

	/* Create descriptor if it does not exists. */
	if(gpmap == NULL || (gpmap->py_end != py_end || gpmap->py_start != py_start))
	{
		gpmap = (struct vmm_phymap_descriptor*)kmalloc(sizeof(struct vmm_phymap_descriptor));
		gpmap->id = vmm_regiondesc_getid();
		gpmap->exclusive = exclusive;
		gpmap->py_start = py_start;
		gpmap->py_end = py_end;
		gpmap->references = 0;

		vmm_regiondesc_add(gpmap);
	}

	gpmap->references++;

	/* Add task region */
	mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));

	mreg->descriptor = gpmap;
	mreg->flags = VMM_MEM_REGION_FLAG_NONE | ((exclusive)? VMM_MEM_REGION_FLAG_EXCLUSIVE : VMM_MEM_REGION_FLAG_NONE);
	mreg->tsk_lstart = (ADDR)laddr;
	mreg->tsk_lend = lend;
	mreg->type = VMM_MEMREGION_MMAP;
	
	vmm_region_add(task, mreg);
	
	return TRUE;
}

/* Unmap a region based on task linear address. */
void vmm_phy_umap(struct pm_task *task, ADDR lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_phymap_descriptor *pmap = NULL;
	UINT32 offset;
	
	lstart = TRANSLATE_ADDR(lstart, ADDR);

	mreg = vmm_region_get(task, (UINT32)lstart);

	if(mreg == NULL) return;

	pmap = (struct vmm_phymap_descriptor*)mreg->descriptor;

	/* Unmap from task address space. */
	for(offset = (UINT32)mreg->tsk_lstart; offset < (UINT32)mreg->tsk_lend; offset += 0x1000)
	{
		/* Page-Out from task address space */
		page_out(task->id, (ADDR)offset, 2);

		/* Return page to VMM */
		vmm_put_page((ADDR)((UINT32)pmap->py_start + (offset - (UINT32)mreg->tsk_lstart)));		
	}

	/* Remove structures */
	pmap->references--;

	if(pmap->references == 0)
	{
		/* Remove region descriptor */
		vmm_regiondesc_remove(pmap);

		kfree(pmap);
	}

	/* Remove region from task. */
	vmm_region_remove(task, mreg);

	kfree(mreg);

	pmap = NULL;
	return;
}



