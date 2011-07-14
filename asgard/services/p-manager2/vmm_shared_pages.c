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
#include "vm.h"
#include "types.h"
#include "task_thread.h"
#include "kmalloc.h"

/* This structure will be used when bringing pages from swap/executable while sharing */
struct vmm_shared_params
{
	UINT32 params[8];
};

/*************************************** CALLBACKS *****************************************************/
UINT32 vmm_share_create_step(struct pm_task *task, struct vmm_shared_params *params, UINT32 ioret);
INT32 vmm_share_create_exe_seek_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_share_create_exe_callback(struct fsio_event_source *iosrc, INT32 ioret);
UINT32 vmm_share_create_swp_callback(struct pm_task *task, UINT32 ioret);
UINT32 vmm_share_map_callback(struct pm_task *task, UINT32 ioret);
void vmm_shared_create_end(struct pm_task *task, struct vmm_shared_params *params);
/*******************************************************************************************************/

/*
Must be invoked when a page fault on a shared region is raised.
Return TRUE is the thread whas blocked, FALSE otherwise.
*/
BOOL vmm_page_shared(struct pm_task *task, ADDR proc_laddr, struct vmm_memory_region *mreg)
{
    struct vmm_shared_descriptor *sdes = NULL;
	ADDR owner_laddr = NULL;
	UINT32 attrib = 0;
    struct pm_task *otsk;
	struct vmm_page_table *optbl = NULL;

	pman_print_dbg("PF: Page is shared \n");

    sdes = (struct vmm_shared_descriptor*)mreg->descriptor;

    attrib = vmm_region_pageperms(mreg);
    owner_laddr = (ADDR)((UINT32)sdes->owner_region->tsk_node.low + ((UINT32)proc_laddr - (UINT32)mreg->tsk_node.low));

	// Page is shared, check if it's present on the owner task. //
	otsk = tsk_get(sdes->owner_task);

	optbl = (struct vmm_page_table *)PHYSICAL2LINEAR(otsk->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(owner_laddr)].b);
		
	if(optbl->pages[PM_LINEAR_TO_TAB(owner_laddr)].entry.ia32entry.present == 0)
	{
		// We won't page out shared pages... if this happened we are screwed. //
        pman_print_dbg("PMAN vmm_page_shared: CRITICAL: Paged out shared page! \n");
	}
	else
	{
		// Map the page //
		pm_page_in(task->id, (ADDR)proc_laddr, (ADDR)PG_ADDRESS(optbl->pages[PM_LINEAR_TO_TAB(owner_laddr)].entry.phy_page_addr), 2, attrib);
		return FALSE;
	}

    return FALSE;
}

/* Removes a reference to a shared memory area. */
void vmm_share_remove(struct pm_task *task, UINT32 lstart)
{
    struct vmm_memory_region *mreg = NULL;
	struct pm_task *child = NULL;
	struct vmm_memory_region *cmreg = NULL, *next = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	UINT32 lend, laddr;
	struct vmm_shared_descriptor *sdes;
	UINT32 i = 0;

	lstart = TRANSLATE_ADDR(lstart,UINT32);

    ma_node *mn = ma_search_low(&task->vmm_info.regions, lstart);

    if(!mn) return;

    mreg = VMM_MEMREG_MEMA2MEMR(mn);

    sdes = (struct vmm_shared_descriptor*)mreg->descriptor;

	if(sdes->owner_task == task->id && sdes->regions != NULL)
	{
		/* 
		If we are invoked with the owner task, it means it's closing! What will happen to
		those poor childs? Let's promote a child to the owner of the shared area!
		*/

        // remove memory region from the original owner
        ma_remove(&task->vmm_info.regions, &sdes->owner_region->tsk_node);
        kfree(sdes->owner_region);

        // promote the first child
        sdes->owner_task = sdes->regions->owner_task;
        sdes->owner_region = sdes->regions;
        sdes->regions = sdes->regions->next;

        mreg = sdes->owner_region;

        if(sdes->regions->next == NULL)
        {
            task = tsk_get(sdes->owner_task); // get the new owner task

            // hmm now this area is no longer shared.. let's remove the shared marks
            // from the info field! (i.e. demote the pages to common pages, but let 
            // them paged in)
            lstart = (UINT32)mreg->tsk_node.low;
	        lend = (UINT32)mreg->tsk_node.high;
	
	        pdir = task->vmm_info.page_directory;

	        while(lstart < lend)
	        {
		        /* Check if page table is present (it wont be swapped) */
		        if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 1)
		        {
			        ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));
		
			        laddr = PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr));

                    vmm_set_flags(task->id, (ADDR)laddr, FALSE, TAKEN_PG_FLAG_SHARED, FALSE); // page is no longer shared!
		        }

		        lstart += 0x1000;
	        }

            // remove the memory region from this task (it's no longer shared!)
            ma_remove(&task->vmm_info.regions, &mreg->tsk_node);
            kfree(mreg);
        }
        return;
	}
	
    // If we got here, we can be sure this shared area
    // is not referenced at all, we remove the share 
    // with confidence and set all pages shared flag to
    // 0.

	lstart = (UINT32)mreg->tsk_node.low;
	lend = (UINT32)mreg->tsk_node.high;
	
	pdir = task->vmm_info.page_directory;

	while(lstart < lend)
	{
		/* Check if page table is present (it wont be swapped) */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 1)
		{
			ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));
		
			laddr = PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr));

			vmm_set_flags(task->id, (ADDR)laddr, FALSE, TAKEN_PG_FLAG_SHARED, FALSE); // page is no longer shared!
		}

		lstart += 0x1000;
	}

	/* Remove region from task */
	ma_remove(&task->vmm_info.regions, &mreg->tsk_node);
    kfree(mreg);

    // we can safely remove the shared descriptor
    // for it's no longer referenced by any memory 
    // region, and it's not contained on any global
    // structures.
    kfree(sdes);
}

/* 
Grant permissions from a given task, for letting other tasks share it's address space. 
RETURNS: TRUE if creation begun.
This function will invoke command_inf callback upon completion.
*/
BOOL vmm_share_create(struct pm_task *task, ADDR laddr, UINT32 length, UINT16 perms)
{
	struct vmm_memory_region *mregit = NULL;
	struct vmm_shared_params *params = NULL;
	struct pm_thread *thr = NULL;

	/* Check address is not above max_addr */
	if(task->vmm_info.max_addr <= (UINT32)laddr + length)
	{
		return FALSE;
	}

	if(perms & PM_SHARE_MEM_WRITE)
		perms = PGATT_WRITE_ENA;
	else
		perms = 0;
	
	/* 
	Remember to set TAKEN_PG_FLAG_SHARED on each shared page.
	*/
	if(task->state != TSK_NORMAL) 
		return FALSE;

	/* Check start is page aligned and length is divisible by 0x1000 */
	if((UINT32)laddr % 0x1000 != 0 || length % 0x1000 != 0) return FALSE;
	
	laddr = TRANSLATE_ADDR(laddr, ADDR);

    /* Check if a region already exists on the same task, overlapping. */
    ma_node *n = ma_collition(&task->vmm_info.regions, (UINT32)laddr, (UINT32)laddr + length);

    if(n)
    {
        return FALSE;
    }
    
	/* Task state will be set to TSK_MMAPPING */
	task->state = TSK_MMAPPING;

	/*
    If a page is not present, we should bring it from swap/executable, or assign a new page.
	*/
	
	/* Begin callback */
	params = (struct vmm_shared_params*)kmalloc(sizeof(struct vmm_shared_params));

	if(params == NULL)
		return FALSE;

	params->params[0] = (UINT32)laddr;                  // param 0 will hold start address (will be modified, use param[3] ath the end)
	params->params[1] = (UINT32)laddr + length;         // param 1 will hold ending address
	params->params[2] = 0xFFFFFFFF;                     // last swap address brought
	params->params[3] = (UINT32)laddr;                  // param 3 will hold start address
	params->params[4] = ((UINT32)perms & 0xFFFFFFFF);   // param 4 will contain permissions
	params->params[5] = 0xFFFFFFFF;                     // last address paged in

	task->io_finished.params[0] = (UINT32)params;

	if(vmm_share_create_step(task, params, IO_RET_OK) == -1)
    {
        // no errors no swap
        return TRUE;
    }
	
    /* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	return TRUE;
}

/*
This is the real callback function.
*/
UINT32 vmm_share_create_step(struct pm_task *task, struct vmm_shared_params *params, UINT32 ioret)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_page_directory *pdir;
	struct vmm_page_table *ptbl;
	struct vmm_shared_descriptor *desc = NULL;
	struct pm_thread *thr;
	UINT32 lstart, lend, pg_addr, last_swp;
	UINT32 filepos, readsize;
	INT32 perms, page_displacement;
	
    if(ioret == IO_RET_ERR)
    {
        vmm_shared_create_end(task, params);
        return 0;
    }
    else
    {
	    /* Set variables based on parameters */
	    lstart = params->params[0];
	    lend = params->params[1];
	    last_swp = params->params[2];
	
	    pdir = task->vmm_info.page_directory;

	    /* Free last swap address if not 0xFFFFFFFF */
	    if(last_swp != 0xFFFFFFFF)
	    {
		    /* Unset IOLOCK */
		    vmm_set_flags(task->id, (ADDR)params->params[5], TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

		    if(last_swp != 0xFFFFFFFE)
			    swap_free_addr(last_swp);			
		
		    /* If we have just brought a lvl 2 page, unmap from pman */
		    if(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b) != LINEAR2PHYSICAL(params->params[5]))
			    vmm_unmap_page(task->id, lstart);
				
		    params->params[2] = 0xFFFFFFFF;
	    }

        // set the page shared flag
        ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));

		vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr)), FALSE, TAKEN_PG_FLAG_SHARED, TRUE);

	    /* 
	    Fetch Swapped or non loaded pages.
	    */
	    for(; lstart < lend; lstart += 0x1000)
	    {
		    /* If page table is not present get us one */
		    if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		    {
			    pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

			    task->vmm_info.page_count++;
		
			    /* Page in onto task adress space */
			    pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 1, PGATT_WRITE_ENA);

			    /* If page table is swapped... bring it from swap. */
			    if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.swapped == 1)
			    {
				    /* Set IOLOCK and PF just in case */
				    vmm_set_flags(task->id, (ADDR)lstart, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

				    task->swp_io_finished.params[0] = (UINT32)params;

				    params->params[0] = lstart;	
				    params->params[2] = pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.addr;
				    params->params[5] = pg_addr;

				    /* Use swp callback */
				    task->swp_io_finished.callback = vmm_share_create_swp_callback;
				    io_begin_task_pg_read( (pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.addr << 3), (ADDR)pg_addr, task);

				    return 0;	// swap OP begun
			    }
		    }
		
		    ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));

		    if(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.ia32entry.present == 0)
		    {
			    /* 
			    Page is not present.
			    If it's swapped, get it.
			    If it has to be loaded, load it.
			    */
			    if(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.swapped == 1)
			    {
				    /* Get a fresh page */
				    pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

                    if(!pg_addr)
                    {
                        vmm_shared_create_end(task, params);
                        return 0;
                    }

				    task->vmm_info.page_count++;
		
				    /* Page in onto task adress space */
				    pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 2, swap_get_perms(task, (ADDR)lstart));

				    /* IO Lock Page */
				    vmm_set_flags(task->id, (ADDR)pg_addr, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

				    /* Begin read operation */
				    task->swp_io_finished.params[0] = (UINT32)params;

				    params->params[0] = lstart;	
				    params->params[2] = ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.addr;
				    params->params[5] = pg_addr;

				    /* Use swp callback */
				    task->swp_io_finished.callback = vmm_share_create_swp_callback;
				    io_begin_task_pg_read( (ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.addr << 3), (ADDR)pg_addr, task);
			    }
			    else if(loader_filepos(task, (ADDR)lstart, &filepos, &readsize, &perms, &page_displacement, NULL))
			    {
				    /* Get a fresh page */
				    pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

                    if(!pg_addr)
                    {
                        vmm_shared_create_end(task, params);
                        return 0;
                    }

				    task->vmm_info.page_count++;
		
				    /* Page in onto task adress space */
				    pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 2, perms);

				    /* IO Lock Page */
				    vmm_set_flags(task->id, (ADDR)pg_addr, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

				    task->swp_io_finished.params[0] = (UINT32)params;

				    params->params[0] = lstart;	
				    params->params[2] = 0xFFFFFFFE;
				    params->params[5] = pg_addr;
				    params->params[6] = ((((UINT16)page_displacement << 16) & 0xFFFF0000) | (readsize & 0x0000FFFF));
				
				    task->io_finished.callback = vmm_share_create_exe_seek_callback;
				    io_begin_seek(&task->io_event_src, filepos);
			    }
			    else
			    {
				    /* Give the tak a fresh page */
				    pg_addr = (UINT32)vmm_get_page(task->id, lstart);

                    if(!pg_addr)
                    {
                        vmm_shared_create_end(task, params);
                        return 0;
                    }

				    task->vmm_info.page_count++;

				    pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 2, PGATT_WRITE_ENA);
			    }
		    }
	    }

	    vmm_shared_create_end(task, params);
    }

	return 1; // No errors, no Swap
}

void vmm_shared_create_end(struct pm_task *task, struct vmm_shared_params *params)
{
    UINT32 lstart, lend, laddr;
    struct vmm_page_directory *pdir;
	struct vmm_page_table *ptbl;
    BOOL idret = FALSE;

    struct vmm_shared_descriptor *desc = (struct vmm_shared_descriptor*)kmalloc(sizeof(struct vmm_shared_descriptor));
    struct vmm_memory_region *mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));

    idret = rb_free_value(&task->vmm_info.regions_id, &mreg->tsk_id_node.value);

    if(params->params[1] == params->params[0] && desc && mreg && idret)
    {
        /* Allocate descriptor */
	    desc->owner_task = task->id;
        desc->regions = NULL;
                	    
	    /* Setup mreg */
        mreg->owner_task = task->id;
	    mreg->descriptor = desc;
	    mreg->type = VMM_MEMREGION_SHARED;
	    mreg->tsk_node.low = params->params[3];
	    mreg->tsk_node.high = lend;
	    mreg->flags = params->params[4];

	    desc->owner_region = mreg;

	    /* Add region to the task */
	    ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
        rb_insert(&task->vmm_info.regions_id, &mreg->tsk_id_node, FALSE);
    }
    else
    {
        // undo what we have done so far 
        lstart = (UINT32)params->params[3];
	    lend = (UINT32)params->params[0];
	
	    pdir = task->vmm_info.page_directory;

	    while(lstart < lend)
	    {
		    /* Check if page table is present (it wont be swapped) */
		    if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 1)
		    {
			    ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));
		
			    laddr = PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr));

			    vmm_set_flags(task->id, (ADDR)laddr, FALSE, TAKEN_PG_FLAG_SHARED, FALSE); // page is no longer shared!
		    }

		    lstart += 0x1000;
	    }
    }

    /* Restore task state */
	task->state = TSK_NORMAL;

	kfree(params);

    /* schedule all threads again */
	struct pm_thread *thr = task->first_thread;

	while(thr != NULL)
	{
		sch_activate(thr);
		thr = thr->next_thread;
	}

	/* Invoke command callback */
	task->command_inf.ret_value = (desc)? ( (task->id << 16) | desc->id) : 0;
	if(task->command_inf.callback != NULL)
			task->command_inf.callback(task, (params->params[1] == params->params[0] && desc && mreg)? IO_RET_OK : IO_RET_ERR);
}

/*
Invoked when an executable seek has completed.
*/
INT32 vmm_share_create_exe_seek_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);

    if(task == NULL)
    {
        return 0;
    }

    struct vmm_shared_params *params = (struct vmm_shared_params*)task->io_finished.params[0];

    if(ioret == IO_RET_ERR)
    {
        vmm_shared_create_end(task, params);
        return 0;
    }

	UINT32 page_displacement, readsize;
	
	page_displacement = ((params->params[6] >> 16) & 0x0000FFFF);
	readsize = (params->params[6] & 0x0000FFFF);
			
	/* Issue Read operation. */
	task->io_finished.callback = vmm_share_create_exe_callback;
	io_begin_read(iosrc, readsize, (ADDR)(params->params[5] + page_displacement));

	return 1;
}

/*
Invoked when an executable read has completed.
*/
INT32 vmm_share_create_exe_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);

    if(task == NULL)
    {
        return 0;
    }

    struct vmm_shared_params *params = (struct vmm_shared_params*)task->io_finished.params[0];

    if(ioret == IO_RET_ERR)
    {
        vmm_shared_create_end(task, params);
        return 0;
    }

	return vmm_share_create_step(task, (struct vmm_shared_params*)task->io_finished.params[0], ioret);
}

/*
Invoked when a swap read has completed.
*/
UINT32 vmm_share_create_swp_callback(struct pm_task *task, UINT32 ioret)
{
    if(task == NULL)
    {
        return 0;
    }

    struct vmm_shared_params *params = (struct vmm_shared_params*)task->io_finished.params[0];

    if(ioret == IO_RET_ERR)
    {
        vmm_shared_create_end(task, params);
        return 0;
    }

	return vmm_share_create_step(task, (struct vmm_shared_params*)task->swp_io_finished.params[0], ioret);
}


/*
Map a memory range on a task to other task shared area.
RETURNS: TRUE if mapping begun.
Invokes command_inf callback upon completion.
*/
BOOL vmm_share_map(UINT32 descriptor_id, struct pm_task *task, ADDR laddr, UINT32 length, UINT16 perms)
{
	struct vmm_memory_region *mreg = NULL;
	UINT32 lstart, lend;
	struct vmm_shared_descriptor *desc = NULL;
	struct vmm_shared_params *params = NULL;
	struct pm_thread *thr = NULL;
    struct pm_task *otask = NULL;

	if(task->state != TSK_NORMAL) 
		return FALSE;

	/* Check address is not above max_addr */
	if(task->vmm_info.max_addr <= (UINT32)laddr + length)
	{
		/* FIXME: Should send an exception signal... */
		return FALSE;
	}

    // check it does not collide with an executable area
    if(loader_collides(task, laddr, laddr + length))
        return FALSE;

	/* Check start is page aligned and length is divisible by PAGE_SIZE */
	if((UINT32)laddr % PAGE_SIZE != 0 || length % PAGE_SIZE != 0) return FALSE;

    otask = tsk_get(descriptor_id >> 16);

    if(!otask || otask->state != TSK_NORMAL) 
		return FALSE;

    // check the region is valid
    rbnode *n = rb_search(&otask->vmm_info.regions_id, (descriptor_id & 0x0000FFFF));

    if(!n) return FALSE;

	/* Get descriptor. */
    mreg = VMM_MEMREG_IDNODE2REG(n);

	desc = (struct vmm_shared_descriptor*)mreg->descriptor;
	
	/* Check size is ok. */
	if(!desc || (UINT32)desc->owner_region->tsk_node.high - (UINT32)desc->owner_region->tsk_node.low < length) 
        return FALSE;
	
	/* Check there is an overlapping region on the task */
	ma_node *n2 = ma_collition(&task->vmm_info.regions, (UINT32)laddr, (UINT32)laddr + length);

    if(n2)
        return FALSE;
    
    params = (struct vmm_shared_params*)kmalloc(sizeof(struct vmm_shared_params));

	if(params == NULL) 
        return FALSE;

	/* Task state will be set to TSK_MMAPPING */
	task->state = TSK_MMAPPING;

	/* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	/* Begin callback */
	params->params[0] = lstart;         // param 0 will hold start address
	params->params[1] = lend;           // param 0 will hold ending address
	params->params[2] = (UINT32)mreg;   // memory region pointer
	params->params[3] = 0xFFFFFFFF;     // last swap address brought
	params->params[4] = lstart;         // param 4 will hold start address
	params->params[5] = ((UINT32)perms & 0xFFFFFFFF);   // param 5 will contain permissions
	params->params[6] = 0xFFFFFFFF;     // last address paged in
	
	task->swp_io_finished.params[0] = (UINT32)params;

	if(vmm_share_map_callback(task, IO_RET_OK) == -1) return FALSE;
	
	return TRUE;
}

/* 
This callback will be issued each time a page is read 
from swap, when mapping a shared area. 
RETURNS: 0 if swap operation begun, 1 if no IO was required, -1 if an error ocurred.
*/
UINT32 vmm_share_map_callback(struct pm_task *task, UINT32 ioret)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_page_directory *pdir, *opdir;
	struct vmm_page_table *ptbl, *optbl;
	struct pm_task *otask = NULL;
	struct vmm_shared_descriptor *desc = NULL;
	struct pm_thread *thr;
	UINT32 lstart, lend, ostart, oend, pg_addr, last_swp;
	struct vmm_shared_params *params = (struct vmm_shared_params*)task->swp_io_finished.params[0];
	
	/* Set variables based on parameters */
	lstart = params->params[0];
	lend = params->params[1];
	last_swp = params->params[3];
    mreg = (struct vmm_memory_region*)params->params[2];
	desc = (struct vmm_shared_descriptor*)mreg->descriptor;

	ostart = (UINT32)desc->owner_region->tsk_node.low;
	oend = (UINT32)desc->owner_region->tsk_node.high;

	/* Free last swap address if not 0xFFFFFFFF */
	if(last_swp != 0xFFFFFFFF)
	{
		swap_free_addr(last_swp);

		/* Unset IOLOCK */
		vmm_set_flags(task->id, (ADDR)params->params[6], TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

		params->params[3] = 0xFFFFFFFF;
	}

	/* Check destination area is not swapped (if it is, I will free them no matter what is there, except for page tables) */
	pdir = task->vmm_info.page_directory;

	for(; lstart < lend; lstart += PAGE_SIZE)
	{
		/* If page table is not present get us one */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		{
			/* Get a fresh page */
			pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

			task->vmm_info.page_count++;
		
			/* Page in onto task adress space */
			pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 1, PGATT_WRITE_ENA);

			/* If page table is swapped... bring it from swap. */
			if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.swapped == 1)
			{
				/* Set IOLOCK and PF just in case */
				vmm_set_flags(task->id, (ADDR)lstart, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

				task->swp_io_finished.params[0] = (UINT32)params;
				
				params->params[0] = lstart;	// param 0 will hold start address
				params->params[1] = lend;	// param 0 will hold ending address
				params->params[2] = (UINT32)desc;	// descriptor pointer
				params->params[3] = pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.addr;
				params->params[6] = pg_addr;

				task->swp_io_finished.callback = swap_free_table_callback;
				io_begin_task_pg_read( (pdir->tables[PM_LINEAR_TO_DIR(lstart)].record.addr << 3), (ADDR)pg_addr, task);

				return 0;	// swap OP begun
			}
		}
		
		ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));

		if(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.ia32entry.present == 0)
		{
			if(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.swapped == 1)
			{
				/* Free the page from swap. */
				swap_free_addr(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.addr);
			}
		}
		else
		{
			pg_addr = PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr);

			/* Pageout this page from task */
			page_out(task->id, (ADDR)lstart, 2);

			/* return page to PMAN */
			vmm_put_page((ADDR)PHYSICAL2LINEAR(pg_addr));	

			task->vmm_info.page_count--;
		}
	}

	lstart = params->params[4];	
	
	/* Allocate new memory region */
	mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));

	if(mreg == NULL)
    {
        thr = task->first_thread;

	    while(thr != NULL)
	    {
		    sch_activate(thr);
		    thr = thr->next_thread;
	    }

        /* Invoke command callback */
        task->command_inf.ret_value = 0;
	    if(task->command_inf.callback != NULL)
			    task->command_inf.callback(task, IO_RET_OK);
        return 0;
    }

	otask = tsk_get(desc->owner_task);
    if(otask == NULL)
    {
        kfree(mreg);

        thr = task->first_thread;

	    while(thr != NULL)
	    {
		    sch_activate(thr);
		    thr = thr->next_thread;
	    }

        /* Invoke command callback */
        task->command_inf.ret_value = 0;
	    if(task->command_inf.callback != NULL)
			    task->command_inf.callback(task, IO_RET_OK);
        return 0;
    }
	opdir = otask->vmm_info.page_directory;

	/* 
	Map pages onto task 
	NOTE: All pages have already been paged out from task, and page tables 
	released from swap or allocated.
	NOTE2: Here I assume all pages on sharing task are paged in, if not... god help us.
	*/
	for(; lstart < lend; lstart += PAGE_SIZE)
	{
		optbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(opdir->tables[PM_LINEAR_TO_DIR(ostart)].b));

		/* Set the same page on page table */
		pm_page_in(task->id, (ADDR)lstart, (ADDR)optbl->pages[PM_LINEAR_TO_TAB(ostart)].entry.phy_page_addr, 2, ((task->swp_io_finished.params[5] & VMM_MEM_REGION_FLAG_WRITE)? PGATT_WRITE_ENA : 0));

		ostart += PAGE_SIZE;
	}

	/* create mreg for the child */
    mreg->owner_task = task->id;
	mreg->descriptor = desc;
	mreg->type = VMM_MEMREGION_SHARED;
	mreg->tsk_node.low = params->params[4];
	mreg->tsk_node.high = lend;
	mreg->flags = params->params[5]; 

	/* Add region to the task */
	ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
    // FIXME: shouldn't I add it to the id tree?

    /* Add region to descriptor regions */
    mreg->next = desc->regions;
    mreg->prev = NULL;
    if(desc->regions)
        desc->regions->prev = mreg;
    desc->regions = mreg;

	/* Restore task state */
	task->state = TSK_NORMAL;

	/* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_activate(thr);
		thr = thr->next_thread;
	}

	/* Invoke command callback */
    task->command_inf.ret_value = 1;
	if(task->command_inf.callback != NULL)
			task->command_inf.callback(task, IO_RET_OK);

	kfree(params);

	return 1; // No errors, no Swap
}



