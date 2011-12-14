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
#include <services/pmanager/services.h>

UINT32 check_low;
UINT32 check_high;
int result;

void vmm_phy_checkpf(rbnode *n)
{
    struct pm_thread *thread = thr_get(n->value2);

    if(n->value >= check_low && n->value < check_high && (thread->flags & (THR_FLAG_PAGEFAULT | THR_FLAG_PAGEFAULT_TBL)))
        result = 1;
}

/*
This function will map a physical address range to the task address space.
*/
BOOL vmm_phy_mmap(struct pm_task *task, ADDR py_start, ADDR py_end, ADDR lstart, ADDR lend, UINT32 pages, UINT32 align, char flags)
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
	struct vmm_memory_region *mreg = NULL;	
    
	/* Check address is not above max_addr */
	if((task->vmm_info.max_addr <= (UINT32)lend)
        || (((UINT32)lstart & 0x00000FFF) || ((UINT32)lend & 0x00000FFF))
        || (py_start != (ADDR)0xFFFFFFFF && (UINT32)py_start < FIRST_PAGE(PMAN_POOL_PHYS) && (UINT32)py_start > 0x1000)
        || !(task->flags & TSK_FLAG_SERVICE))
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR1\n");
		return FALSE;
    }

    // alloc memory region
    mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));
    
    if(!mreg)
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR2\n");
        return FALSE;
    }
    
	mreg->next = NULL;
    mreg->prev = NULL;
	mreg->owner_task = task->id;

    // find a free id for the memory region
    if(!rb_free_value(&task->vmm_info.regions_id, &mreg->tsk_id_node.value))
	{
        pman_print_dbg("PMAN vmm_phy_mmap ERR3\n");
		kfree(mreg);
		return FALSE;
	}
    
    lstart = TRANSLATE_ADDR(lstart, ADDR);
	lend = TRANSLATE_ADDR(lend, ADDR);

    // check lstart and lend are "safe" to map
    if(loader_collides(task, lstart, lend))
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR4\n");
		kfree(mreg);
		return FALSE;
	}

    // check there are no other memory regions overlapping on the same task
    if(ma_collition(&task->vmm_info.regions, (UINT32)lstart, (UINT32)lend))
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR5\n");
		kfree(mreg);
		return FALSE;
	}
    
	/* Check a thread is not page faulting on the region. */
    check_low = (UINT32)laddr;
    check_high = (UINT32)lend;
    result = 0;
    rb_inorder(&task->vmm_info.wait_root, vmm_phy_checkpf);

    if(result)
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR6\n");
		kfree(mreg);
		return FALSE;
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

				if((tentry->data.b_pg.eflags & ~TAKEN_EFLAG_SERVICE) != 0 || (tentry->data.b_pg.flags & (TAKEN_PG_FLAG_FILE | TAKEN_PG_FLAG_LIBEXE | TAKEN_PG_FLAG_IO)))
                {
                    pman_print_dbg("PMAN vmm_phy_mmap ERR7\n");
                    kfree(mreg);
					return FALSE;	// taken and cannot be mapped
                }
				
				/* Unmap page and return physical address to PMAN */
				vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.phy_page_addr)));

				page_out(task->id, (ADDR)laddr, 2);

                task->vmm_info.page_count--;
			}
			else if(tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.record.swapped == 1)
			{
                // page is swapped.. remove it from swap
                swap_free_addr( tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.record.addr );
                tbl->pages[PM_LINEAR_TO_TAB(laddr)].entry.record.swapped = 0;
			}
		}
        /* If page table is not present check it's not swapped */
		else if(pdir->tables[PM_LINEAR_TO_DIR(laddr)].record.swapped == 1)
		{
            pman_print_dbg("PMAN vmm_phy_mmap page table swapped\n");
            // we should bring it back from swap...
            kfree(mreg);
			return FALSE;	// page table is swapped
		}
		laddr += PAGE_SIZE;
	}
    
    pmap = (struct vmm_phymap_descriptor*)kmalloc(sizeof(struct vmm_phymap_descriptor));

    if(!pmap)
    {
        pman_print_dbg("PMAN vmm_phy_mmap ERR8\n");
        kfree(mreg);
        kfree(pmap);
		return FALSE;
    }
    
    /*
    Get the memory
    */
    if((UINT32)py_start == 0xFFFFFFFF)
    {
        if(align == 0)
            pstart = (UINT32)pya_get_pages(((flags & PM_PMAP_LOW_MEM)? &vmm.low_pstack : &vmm.pstack), pages, (flags & PM_PMAP_IO));
        else
            pstart = (UINT32)pya_get_pages_aligned(((flags & PM_PMAP_LOW_MEM)? &vmm.low_pstack : &vmm.pstack), pages, (flags & PM_PMAP_IO), align);
        
        if(pstart == NULL)
        {
            pman_print_dbg("PMAN vmm_phy_mmap ERR9\n");
		    kfree(mreg);
            kfree(pmap);
		    return FALSE;
	    }
        pend = pstart + (pages << 12);
        py_start = (ADDR)LINEAR2PHYSICAL(pstart);
        py_end = (ADDR)LINEAR2PHYSICAL(pend);
    }
    else
    {
        // Check if it's already mapped to another task (this should be faster than pya_get_pages_addr) 
        // that's why I check first even though pya_get_pages_addr won't return a used page.
        if(ma_collition(&task->vmm_info.regions, (UINT32)py_start, (UINT32)py_end))
        {
            pman_print_dbg("PMAN vmm_phy_mmap ERR10\n");
		    kfree(mreg);
            kfree(pmap);
		    return FALSE;
	    }

        if((UINT32)py_start != 0xFFFFFFFF && pya_get_pages_addr(vmm_addr_stack((ADDR)pstart), (ADDR)pstart, pages, (flags & PM_PMAP_IO)) == NULL)
        {
            pman_print_dbg("PMAN vmm_phy_mmap ERR11\n");
            kfree(mreg);
            kfree(pmap);
		    return FALSE;
        }
    }
    	    
	/* 
	Ok, memory range is available (both physical and linear)... 
    great! lets have those physical pages!! 
	*/
	laddr = TRANSLATE_ADDR(lstart, UINT32);
    pstart = PHYSICAL2LINEAR(py_start);
    int att = 0;

    if(flags & PM_PMAP_CACHEDIS)
        att = PGATT_CACHE_DIS;
    if(flags & PM_PMAP_WRITE)
        att |= PGATT_WRITE_ENA;
    
	/* Now Map physical pages onto the task address space */
	while(pstart < pend)
	{
		/* Check page table is present, if not give it one. */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		{
			pg_tbl = vmm_get_tblpage(task->id, (UINT32)lstart);

			page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_tbl), 1, PGATT_WRITE_ENA);

			task->vmm_info.page_count++;
		}

		/* Map the page. */
		page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pstart), 2, att);

		/* Set taken. */
		tentry = vmm_taken_get((ADDR)pstart);

		tentry->data.b_pg.flags |= TAKEN_PG_FLAG_PHYMAP;

		/* Unmap page from PMAN address space. */
		vmm_unmap_page(task->id, (UINT32)lstart);

		pstart += 0x1000;
		lstart = (ADDR)((UINT32)lstart + 0x1000);
	}

	pmap->area.low = (UINT32)py_start;
	pmap->area.high = (UINT32)py_end;
    pmap->flags = flags;
	pmap->region = mreg;

    // insert the descriptor on the global structure
	ma_insert(&vmm.phy_mem_areas, &pmap->area);

    /* Add task region */
	mreg->descriptor = pmap;
	mreg->flags = VMM_MEM_REGION_FLAG_NONE | ((flags & PM_PMAP_WRITE)? VMM_MEM_REGION_FLAG_WRITE : VMM_MEM_REGION_FLAG_NONE);
	mreg->tsk_node.low = laddr;
	mreg->tsk_node.high = (UINT32)lend;
	mreg->type = VMM_MEMREGION_MMAP;
	
	rb_insert(&task->vmm_info.regions_id, &mreg->tsk_id_node, FALSE);
    ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
    
    if(task->command_inf.callback != NULL) 
		task->command_inf.callback(task, IO_RET_OK, (UINT32)py_start);
	
	return TRUE;
}

/* Unmap a region based on task linear address. */
void vmm_phy_umap(struct pm_task *task, ADDR lstart, int free_io)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_phymap_descriptor *pmap = NULL;
	UINT32 offset, pstart;
	
	lstart = TRANSLATE_ADDR(lstart, ADDR);

    ma_node *n = ma_search_low(&task->vmm_info.regions, (UINT32)lstart);

    if(!n) return;

	mreg = VMM_MEMREG_MEMA2MEMR(n);

	if(mreg->type != VMM_MEMREGION_MMAP) return;

	pmap = (struct vmm_phymap_descriptor*)mreg->descriptor;

	/* Unmap from task address space. */
	for(offset = (UINT32)mreg->tsk_node.low; offset < (UINT32)mreg->tsk_node.high; offset += PAGE_SIZE)
	{
		/* Page-Out from task address space */
		page_out(task->id, (ADDR)offset, 2);
	}

    pstart = PHYSICAL2LINEAR(pmap->area.low);

    /* return memory */
    vmm_put_pages((ADDR)pstart, ((pmap->area.high - pmap->area.low) >> 12), (pmap->flags & PM_PMAP_IO), free_io);

	/* Remove structures */
	ma_remove(&vmm.phy_mem_areas, &pmap->area);

    kfree(pmap);

	/* Remove region from task. */
	rb_remove(&task->vmm_info.regions_id, &mreg->tsk_id_node);
    ma_remove(&task->vmm_info.regions, &mreg->tsk_node);

	kfree(mreg);

    if(task->command_inf.callback != NULL) 
		task->command_inf.callback(task, IO_RET_OK, 0);

	return;
}



