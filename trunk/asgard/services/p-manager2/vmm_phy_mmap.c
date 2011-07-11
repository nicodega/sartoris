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

BOOL wmm_phy_overlaps(ma_node *n)
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
	struct vmm_memory_region *mreg;	

	/* Check address is not above max_addr */
	if((task->vmm_info.max_addr <= (UINT32)lend)
        || ((UINT32)lstart % 0x1000 != 0 || ((UINT32)lend) % 0x1000 != 0)
        || ((UINT32)py_start < FIRST_PAGE(PMAN_POOL_PHYS) && (UINT32)py_start > 0x1000)
        || !(task->flags & TSK_FLAG_SERVICE))
		return FALSE;

    // frind a free ID for MREG
    mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));
    
    if(!mreg)
        return FALSE;

	mreg->next = NULL;
    mreg->prev = NULL;
	mreg->owner_task = task->id;

    // find a free if for the memory region
    if(!rb_free_value(&task->vmm_info.regions_id, &mreg->tsk_id_node.value))
	{
		kfree(mreg);
		return FALSE;
	}

    lstart = TRANSLATE_ADDR(lstart, ADDR);
	lend = TRANSLATE_ADDR(lend, ADDR);

    // check lstart and lend are "safe" to map
    if(loader_collides(task, lstart, lend))
    {
		kfree(mreg);
		return FALSE;
	}

    // check there are no other memory regions overlapping on the same task
    if(ma_collition(&task->vmm_info.regions, (UINT32)lstart, (UINT32)lend))
    {
		kfree(mreg);
		return FALSE;
	}
    	
	/* Check a thread is not page faulting on the region */
    check_low = (UINT32)laddr;
    check_high = (UINT32)lend;
    result = 0;
    rb_inorder(&task->vmm_info.wait_root, wmm_phy_checkpf);

    if(result)
    {
		kfree(mreg);
		return FALSE;
	}
    
    // Check if it's already mapped to another task (or the same?)
    result = 0;
    cexclusive = exclusive;
    ma_overlaps(&vmm.phy_mem_areas, (UINT32)py_start, (UINT32)py_end, wmm_phy_overlaps);

    if(result)
    {
		kfree(mreg);
		return FALSE;
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
			if(tentry->data.b_pg.dir == 1 || tentry->data.b_pg.tbl == 1 || (tentry->data.b_pg.flags & (TAKEN_PG_FLAG_SHARED | TAKEN_PG_FLAG_FILE | TAKEN_PG_FLAG_PMAN)))
			{
                // try to move the page (for now we won't move shared and file pages)!
                // PMAN pages cannot be moved because pman has a 1 on 1 mapping with memory on it's
                // linear memory.
                if(tentry->data.b_pg.dir == 1)
                {
                    struct pm_task *t = tsk_get(tentry->data.b_pdir.taskid);

                    /*
                    FIXME: What if here we got a page on the physical range??
                    */
                    pg_addr = (UINT32)vmm_get_dirpage(t->id);
                    
                    if(pg_addr == NULL)
                    {
                        kfree(mreg);
				        return FALSE;
                    }

                    pm_page_in(t->id, 0, (ADDR)LINEAR2PHYSICAL(pg_addr), 0, PGATT_WRITE_ENA);

                    UINT32 *src = (UINT32*)pstart, 
                           *dst = (UINT32*)pg_addr;
                    int i = 0;

                    for(i = 0; i < 0x400; i++)
                        dst[i] = src[i];
                                        
                    pdir = t->vmm_info.page_directory;
                                        
                    t->vmm_info.page_directory = (struct vmm_page_directory *)pg_addr; 
                    				    
				    /* Put page on VMM */
				    vmm_put_page((ADDR)pdir);
                }
                else if(tentry->data.b_pg.tbl == 1)
                {   
                    if(tentry->data.b_ptbl.eflags & (TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF))
                    {
                        kfree(mreg);
				        return FALSE;
                    }

                    struct pm_task *t = tsk_get(tentry->data.b_ptbl.taskid);

                    /*
                    FIXME: What if here we got a page on the physical range??
                    */
                    pg_addr = (UINT32)vmm_get_tblpage(t->id, tentry->data.b_ptbl.dir_index * 0x400000);
                
                    if(pg_addr == NULL)
                    {
                        kfree(mreg);
				        return FALSE;
                    }

                    pm_page_in(t->id, (ADDR)PG_ADDRESS(tentry->data.b_ptbl.dir_index * 0x400000), (ADDR)LINEAR2PHYSICAL(pg_addr), 1, PGATT_WRITE_ENA);

                    UINT32 *src = (UINT32*)pstart, 
                           *dst = (UINT32*)pg_addr;
                    int i = 0;

                    for(i = 0; i < 0x400; i++)
                        dst[i] = src[i];
                    				    
				    /* Put page on VMM */
				    vmm_put_page((ADDR)pstart);
                }
                else
                {
                    kfree(mreg);
				    return FALSE;	// taken and cannot be mapped
                }
			}

			if(!(tentry->data.b_pg.flags & TAKEN_PG_FLAG_PHYMAP))
			{
				/* 
				It's being used by a task but not physically mapped or shared... Unmap it!! :@
				NOTE: If a thread was working on this page, it'll page fault wen runned again.
				*/
				assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(TRANSLATE_ADDR(pstart,UINT32))]->entries[PM_LINEAR_TO_TAB(TRANSLATE_ADDR(pstart,UINT32))];

                if(tsk_get(assigned->task_id) == NULL)
                {
                    kfree(mreg);
                    return FALSE;
                }

                struct pm_task *t = tsk_get(assigned->task_id);
				pdir = t->vmm_info.page_directory;
				ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[assigned->dir_index].b));

				/* Check page is not dirty. */
				if(ptbl->pages[tentry->data.b_pg.tbl_index].entry.ia32entry.dirty == 1)
                {
                    kfree(mreg);
					return FALSE;
                }

				pg_addr = PG_ADDRESS(PHYSICAL2LINEAR(ptbl->pages[tentry->data.b_pg.tbl_index].entry.phy_page_addr));
				
				/* Page out from task */
				page_out(assigned->task_id, (ADDR)(assigned->dir_index * 0x400000 + tentry->data.b_pg.tbl_index * PAGE_SIZE), 2);

				/* Put page on VMM */
				vmm_put_page((ADDR)pg_addr);

                t->vmm_info.page_count--;
			}
		}
		pstart += PAGE_SIZE;
	}

    pdir = task->vmm_info.page_directory;

	/* Check task address space can be mapped. */
	while(laddr < (UINT32)lend)
	{
		if(pdir->tables[PM_LINEAR_TO_DIR(laddr)].ia32entry.present == 1)
		{
			tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(laddr)].b));

			if(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.ia32entry.present == 1)
			{
				/* Page is present, check it's not IOLOCKED, etc */
				tentry = vmm_taken_get((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.phy_page_addr)));

				if(tentry->data.b_pg.eflags != 0 || (tentry->data.b_pg.flags & TAKEN_PG_FLAG_FILE))
                {
                    kfree(mreg);
					return FALSE;	// taken and cannot be mapped
                }
				
				/* Unmap table and return physical address to PMAN */
				vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.phy_page_addr)));

				page_out(task->id, (ADDR)laddr, 2);

                task->vmm_info.page_count--;
			}
			else if(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.record.swapped == 1)
			{
                kfree(mreg);
				return FALSE;	// page is swapped
			}
		}
        /* If page table is not present check it's not swapped */
		else if(pdir->tables[PM_LINEAR_TO_DIR(laddr)].record.swapped == 1)
		{
            kfree(mreg);
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

		/* Map the page. */
		page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pstart), 2, (PGATT_WRITE_ENA | PGATT_CACHE_DIS));

		/* Set taken. */
		tentry = vmm_taken_get((ADDR)pstart);

		tentry->data.b = 0;
		tentry->data.b_pg.taken = 1;
		tentry->data.b_pg.flags = TAKEN_PG_FLAG_PHYMAP;

		/* Unmap page from PMAN address space. */
		vmm_unmap_page(task->id, pstart);

		pstart += 0x1000;
		lstart = (ADDR)((UINT32)lstart + 0x1000);
	}

    // is there a physical map descriptor with exactly the same bounds?
    ma_node *n = ma_search(&vmm.phy_mem_areas, (UINT32)py_start, (UINT32)py_end);
            
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
		pmap->area.low = (UINT32)py_start;
		pmap->area.high = (UINT32)py_end;
		pmap->references = 0;

        // insert the descriptor on the global structure
		ma_insert(&vmm.phy_mem_areas, &pmap->area);
	}

	pmap->references++;

	/* Add task region */
	mreg->descriptor = pmap;
	mreg->flags = VMM_MEM_REGION_FLAG_NONE | ((exclusive)? VMM_MEM_REGION_FLAG_EXCLUSIVE : VMM_MEM_REGION_FLAG_NONE);
	mreg->tsk_node.low = laddr;
	mreg->tsk_node.high = (UINT32)lend;
	mreg->type = VMM_MEMREGION_MMAP;
	
	rb_insert(&task->vmm_info.regions_id, &mreg->tsk_id_node, FALSE);
    ma_insert(&task->vmm_info.regions, &mreg->tsk_node);

    if(task->command_inf.callback != NULL) 
		task->command_inf.callback(task, IO_RET_OK);
	
	return TRUE;
}

/* Unmap a region based on task linear address. */
void vmm_phy_umap(struct pm_task *task, ADDR lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_phymap_descriptor *pmap = NULL;
	UINT32 offset;
	
	lstart = TRANSLATE_ADDR(lstart, ADDR);

    ma_node *n = rb_search_low(&task->vmm_info.regions, (UINT32)lstart);

    if(!n) return;

	mreg = VMM_MEMREG_MEMA2MEMR(n);

	if(mreg->type != VMM_MEMREGION_MMAP) return;

	pmap = (struct vmm_phymap_descriptor*)mreg->descriptor;

	/* Unmap from task address space. */
	for(offset = (UINT32)mreg->tsk_node.low; offset < (UINT32)mreg->tsk_node.high; offset += PAGE_SIZE)
	{
		/* Page-Out from task address space */
		page_out(task->id, (ADDR)offset, 2);

		/* Return page to VMM */
		vmm_put_page((ADDR)((UINT32)pmap->area.low + (offset - (UINT32)mreg->tsk_node.low)));

        task->vmm_info.page_count--;
	}

	/* Remove structures */
	pmap->references--;

	if(pmap->references == 0)
	{
		/* Remove region descriptor */
		ma_remove(&vmm.phy_mem_areas, &pmap->area);

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
	rb_remove(&task->vmm_info.regions_id, &mreg->tsk_id_node);
    ma_remove(&task->vmm_info.regions, &mreg->tsk_node);

	kfree(mreg);

    if(task->command_inf.callback != NULL) 
		task->command_inf.callback(task, IO_RET_OK);

	return;
}



