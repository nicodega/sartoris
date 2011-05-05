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
#include "rb.h"
#include "loader.h"

UINT32 check_low;
UINT32 check_high;
int result;
int cexclusive;

void wmm_phy_checkpf(rbnode *n)
{
    struct pm_thread *thread = thr_get(n->value2);

    if(n->value >= check_low && n->value < check_high && (thread->flags & (THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL)))
        result = 1;
}

BOOL wmm_phy_overlaps(rbnode *n)
{
    struct vmm_phymap_descriptor *pmap = MAREA2PHYMDESC(n);

    if(pmap->exclusive || cexclusive)
    {
        result = 1;
        return FALSE;
    }
    return TRUE;
}

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
	struct vmm_phymap_descriptor *pmap = NULL;
	ADDR pg_tbl = NULL, pg = NULL;
	struct vmm_page_table *tbl = NULL;
	UINT32 maps = 0;
	struct pm_thread *thread = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	struct vmm_pman_assigned_record *assigned = NULL;

	/* Check address is not above max_addr */
	if((task->vmm_info.max_addr <= (UINT32)lend)
        || ((UINT32)lstart % 0x1000 != 0 || ((UINT32)lend) % 0x1000 != 0)
        || ((UINT32)py_start < FIRST_PAGE(PMAN_POOL_PHYS) && (UINT32)py_start > 0x1000))
		return FALSE;

    lstart = TRANSLATE_ADDR(lstart, ADDR);
	lend = TRANSLATE_ADDR(lend, ADDR);

    // check lstart and lend are "safe" to map
    if(loader_collides(task, lstart, lend))
        return FALSE;

    // check there are no other memory regions overlapping on the same task
    if(ma_collition(&task->regions, py_start, py_end))
        return FALSE;
    	
	
	/* Check a thread is not page faulting on the region */
    check_low = (UINT32)laddr;
    check_high = (UINT32)lend;
    result = 0;
    rb_inorder(&task->wait_root, wmm_phy_checkpf);

    if(result)
        return FALSE;
    
    // Check if it's already mapped to another task (or the same?)
    ma_node *n = ma_collition(&vmm.phy_mem_areas, py_start, py_end);
    result = 0;
    cexclusive = exclusive;
    ma_overlaps(&vmm.phy_mem_areas, py_start, py_end, wmm_phy_overlaps);

    if(result)
        return FALSE;
    
	/* 
	Check if required physical range is mappeable.
	
	It will be mappeable if:
		- It's not already assigned to a task.
		- It's assigned to a task, but its an mmaping in non exclusive mode.
	*/
	while(pstart < pend)
	{
		tentry = vmm_taken_get((ADDR)PHISICAL2LINEAR(pstart));

		if(tentry->data.b_pg.taken == 1)
		{
			if(tentry->data.b_pg.dir == 1 || tentry->data.b_pg.tbl == 1 || (tentry->data.b_pg.flags & (TAKEN_PG_FLAG_FILE | TAKEN_PG_FLAG_PMAN)))
			{
				return FALSE;	// taken and cannot be mapped
			}

			if(!(tentry->data.b_pg.flags & TAKEN_PG_FLAG_SHARED) && !(tentry->data.b_pg.flags & TAKEN_PG_FLAG_PHYMAP))
			{
				/* 
				It's being used by a task but not physically mapped or shared... Unmap it!! :@
				NOTE: If a thread was working on this page, it'll page fault wen runned again.
				*/
				assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(PHYSICAL2LINEAR(TRANSLATE_ADDR(pstart)))]->entries[PM_LINEAR_TO_TAB(PHYSICAL2LINEAR(TRANSLATE_ADDR(pstart)))];

                if(tsk_get(assigned->task_id) == NULL) return 0;

				pdir = tsk_get(assigned->task_id)->vmm_info.page_directory;
				ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[assigned->dir_index].b));

				/* Check page is not dirty. */
				if(ptbl->pages[tentry->data.b_pg.tbl_index].entry.ia32entry.dirty == 1)
					return FALSE;

				pg_addr = PG_ADDRESS(PHYSICAL2LINEAR(ptbl->pages[tentry->data.b_pg.tbl_index].entry.phy_page_addr));
				
				/* Page out from task */
				page_out(assigned->task_id, (ADDR)(assigned->dir_index * 0x400000 + tentry->data.b_pg.tbl_index * PAGE_SIZE), 2);

				/* Put page on VMM */
				vmm_put_page((ADDR)pg_addr);
			}
		}
		pstart += PAGE_SIZE;
	}

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
		laddr += PAGE_SIZE;
	}
    
	/* 
	Ok, memory range is available (both physical and linear)... 
    great! lets have those physical pages!! 
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

    // is there a physical map descripto with exactly the same bounds?
    n = ma_search(&vmm.phy_mem_areas, py_start, py_end);

    mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));
    mreg->next = NULL;
    mreg->prev = NULL;

    if(!mreg)
        return FALSE;

    // find a free if for the memory region
    if(!rb_free_value(task->regions_id, &mreg->tsk_id_node.value))
	{
		kfree(mreg);
		return FALSE;
	}
    
    if(n)
    {
        pmap = MAREA2PHYMDESC(n);

        mreg->next = pmap->regions;
        mreg->prev = NULL;
        pmap->regions->prev = mreg;
        pmap->regions = mreg;
    }
    else
    {
		pmap = (struct vmm_phymap_descriptor*)kmalloc(sizeof(struct vmm_phymap_descriptor));

        if(!pmap)
        {
            kfree(mreg);
            return FALSE;
        }

		pmap->exclusive = exclusive;
		pmap->area.low = py_start;
		pmap->area.high = py_end;
		pmap->references = 0;

        // insert the descriptor on the global structure
		ma_insert(&vmm.phy_mem_areas, &pmap->area);
	}

	pmap->references++;

	/* Add task region */
	mreg->descriptor = pmap;
	mreg->flags = VMM_MEM_REGION_FLAG_NONE | ((exclusive)? VMM_MEM_REGION_FLAG_EXCLUSIVE : VMM_MEM_REGION_FLAG_NONE);
	mreg->tsk_node.low = (ADDR)laddr;
	mreg->tsk_node.high = lend;
	mreg->type = VMM_MEMREGION_MMAP;
	
	rb_insert(&task->regions_id, &mreg->tsk_id_node);
    ma_insert(&task->regions, &mreg->tsk_node);
	
	return TRUE;
}

/* Unmap a region based on task linear address. */
void vmm_phy_umap(struct pm_task *task, ADDR lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_phymap_descriptor *pmap = NULL;
	UINT32 offset;
	
	lstart = TRANSLATE_ADDR(lstart, ADDR);

    ma_node *n = rb_search_low(&task->regions);

    if(!n) return;

	mreg = VMM_MEMREG_MEMA2MEMR(n);

	if(mreg->type != VMM_MEMREGION_MMAP) return;

	pmap = (struct vmm_phymap_descriptor*)mreg->descriptor;

	/* Unmap from task address space. */
	for(offset = (UINT32)mreg->tsk_lstart; offset < (UINT32)mreg->tsk_lend; offset += PAGE_SIZE)
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
		a_remove(&vmm.phy_mem_areas, &pmap->area);

		kfree(pmap);
	}
    else
    {
        // remove from regions list
        if(mreg->prev == NULL)
        {
            pmap->regions = mreg->next;
            if(pmap->regions)
                pmap->regions->prev = NULL;
        }
        else if(mreg->next == NULL)
        {
            mreg->prev->next = NULL;
        }
        else
        {
            mreg->prev->next = mreg->next;
            mreg->next->prev = mreg->prev;
        }
    }

	/* Remove region from task. */
	rb_remove(&task->regions_id, &mreg->tsk_id_node);
    ma_remove(&task->regions, &mreg->tsk_node);

	kfree(mreg);

	return;
}



