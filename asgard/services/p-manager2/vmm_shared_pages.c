
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
/*******************************************************************************************************/

/*
Returns the owner of the shared page for the specified task/page combination.
*/
struct pm_task *vmm_shared_getowner(struct pm_task *task, ADDR proc_laddr, ADDR *owner_laddr, UINT32 *attrib)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_shared_descriptor *sdes = NULL;

	/* Get memory region for this address */
	mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if(mreg->tsk_lstart <= proc_laddr && mreg->tsk_lend > proc_laddr)
			break;
		mreg = mreg->next;
	}

	/* Get Sharing descriptor for this region */
	sdes = (struct vmm_shared_descriptor*)mreg;

	if(attrib != NULL) *attrib = vmm_region_pageperms(mreg);

	if(owner_laddr != NULL) *owner_laddr = (ADDR)((UINT32)sdes->shared_laddr_start + ((UINT32)proc_laddr - (UINT32)mreg->tsk_lstart));

	return tsk_get(sdes->owner_task);
}

/*
Returns TRUE if page is shared.
*/
BOOL vmm_is_shared(struct pm_task *task, ADDR proc_laddr)
{
	struct vmm_memory_region *mreg = NULL;

	/* Get memory region for this address */
	mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if(mreg->tsk_lstart <= proc_laddr && mreg->tsk_lend > proc_laddr)
			break;
		mreg = mreg->next;
	}

	return mreg != NULL;
}

/* Removes a reference to a shared memory area. */
void vmm_share_remove(struct pm_task *task, struct vmm_memory_region *mreg)
{
	struct pm_task *child = NULL;
	struct vmm_memory_region *cmreg = NULL, *next = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	UINT32 lstart, lend, laddr;
	struct vmm_shared_descriptor *sdes = (struct vmm_shared_descriptor*)mreg->descriptor;
	UINT32 i = 0;

	if(sdes->owner_task == task->id)
	{
		/* 
		If we are invoked with the owner task, it means it's closing! What will happen to
		those poor childs? Shal I let them PF or find a new owner for the sharing?....
		For now... I'll remove all sharings!
		*/
		
		/* Find all regions referencing this share. (This is expensive) */
		for(i = 0; i < MAX_TSK; i++)
		{
			child = tsk_get(i);

			if(task->id != i && child != NULL && child->state != TSK_NOTHING && child->state != TSK_KILLING && child->state != TSK_KILLED)
			{
				/* Go through regions. */
				cmreg = child->vmm_inf.regions.first;

				while(cmreg != NULL)
				{
					next = cmreg->next;
					if(cmreg->type == VMM_MEMREGION_FMAP && (struct vmm_shared_descriptor*)cmreg->descriptor == sdes)
						vmm_share_remove(task, cmreg);
					
					cmreg = next;
				}
			}
		}

		return;
	}
	
	/* Page-out pages */
	lstart = (UINT32)mreg->tsk_lstart;
	lend = (UINT32)mreg->tsk_lend;
	
	pdir = task->vmm_inf.page_directory;

	while(lstart < lend)
	{
		/* Check if page table is present (it wont be swapped) */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 1)
		{
			ptbl = (struct vmm_page_table*)PG_ADDRESS(PHYSICAL2LINEAR(pdir->tables[PM_LINEAR_TO_DIR(lstart)].b));
		
			laddr = PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.phy_page_addr));

			/* Page out */
			page_out(task->id, (ADDR)PG_ADDRESS(lstart), 2);

			/* 
			NOTE: I will put page onto the page stack only if this is the owner task.
			*/
			if(sdes->owner_task == task->id)
				vmm_put_page((ADDR)laddr);			
		}

		lstart += 0x1000;
	}

	/* Remove region from task */
	vmm_region_remove(task, mreg);

	/* Free region structure */
	kfree(mreg);

	/* Decrement descriptor references. */
	sdes->references--;

	if(sdes->references == 0)	// References will only reach 0 when the shared area is dismissed, hence pages have been 
	{							// unmapped and returned to PMAN

		/* Free descriptor */
		vmm_regiondesc_remove(sdes);

		kfree(sdes);
	}
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
	if(task->vmm_inf.max_addr <= (UINT32)laddr + length)
	{
		/* FIXME: Should send an exception signal... */
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
	mregit = task->vmm_inf.regions.first;

	while(mregit != NULL)
	{
		if( ((UINT32)laddr <= (UINT32)mregit->tsk_lstart && ((UINT32)laddr + length) >= (UINT32)mregit->tsk_lstart) 
			|| ((UINT32)laddr <= (UINT32)mregit->tsk_lend && ((UINT32)laddr + length) >= (UINT32)mregit->tsk_lstart) )
		{
			return FALSE;
		}

		mregit = mregit->next;
	}

	/* Task state will be set to TSK_MMAPPING */
	task->state = TSK_MMAPPING;

	/* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	/*
    If a page is not present, we should bring it from swap/executable 
	*/
	
	/* Begin callback */
	params = (struct vmm_shared_params*)kmalloc(sizeof(struct vmm_shared_params));

	if(params == NULL)
		return FALSE;

	params->params[0] = (UINT32)laddr;	                // param 0 will hold start address
	params->params[1] = (UINT32)laddr + length;           // param 0 will hold ending address
	params->params[2] = 0xFFFFFFFF;                       // last swap address brought
	params->params[3] = (UINT32)laddr;	                    // param 3 will hold start address
	params->params[4] = ((UINT32)perms & 0xFFFFFFFF);		// param 4 will contain permissions
	params->params[5] = 0xFFFFFFFF;                       // last address paged in

	task->io_finished.params[0] = (UINT32)params;

	if(vmm_share_create_step(task, params, IO_RET_OK) == -1) return FALSE;
	
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
	
	/* Set variables based on parameters */
	lstart = params->params[0];
	lend = params->params[1];
	last_swp = params->params[2];
	
	pdir = task->vmm_inf.page_directory;

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

	/* 
	Fetch Swapped or non loaded pages.
	*/
	for(; lstart < lend; lstart += 0x1000)
	{
		/* If page table is not present get us one */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		{
			/* Get a fresh page */
			pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

			task->vmm_inf.page_count++;
		
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
			If it has to be loaded, get it.
			*/
			if(ptbl->pages[PM_LINEAR_TO_TAB(lstart)].entry.record.swapped == 1)
			{
				/* Get a fresh page */
				pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

				task->vmm_inf.page_count++;
		
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
			else if(loader_filepos(task, (ADDR)lstart, &filepos, &readsize, &perms, &page_displacement))
			{
				/* Get a fresh page */
				pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

				task->vmm_inf.page_count++;
		
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

				task->vmm_inf.page_count++;

				pm_page_in(task->id, (ADDR)lstart, (ADDR)LINEAR2PHYSICAL(pg_addr), 2, PGATT_WRITE_ENA);
			}
		}
	}

	/* Allocate descriptor */
	desc = (struct vmm_shared_descriptor*)kmalloc(sizeof(struct vmm_shared_descriptor));

	desc->owner_task = task->id;
	desc->references = 1;
	desc->shared_laddr_end = (ADDR)params->params[1];
	desc->shared_laddr_start = (ADDR)lend;
	desc->type = VMM_MEMREGION_SHARED;

	vmm_regiondesc_add(desc);

	/* Allocate memory region */
	mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));

	/* Setup mreg */
	mreg->descriptor = desc;
	mreg->type = VMM_MEMREGION_SHARED;
	mreg->tsk_lstart = (ADDR)params->params[3];
	mreg->tsk_lend = (ADDR)lend;
	mreg->flags = params->params[4]; 

	/* Add region to the task */
	vmm_region_add(task, mreg);

	/* Restore task state */
	task->state = TSK_NORMAL;

	/* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_activate(thr);
		thr = thr->next_thread;
	}

	kfree(params);

	/* Invoke command callback */
	task->command_inf.ret_value = desc->id;
	if(task->command_inf.callback != NULL)
			task->command_inf.callback(task, IO_RET_OK);

	return 1; // No errors, no Swap
}

/*
Invoked when an executable seek has completed.
*/
INT32 vmm_share_create_exe_seek_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);
	UINT32 page_displacement, readsize;
	struct vmm_shared_params *params = (struct vmm_shared_params*)task->io_finished.params[0];

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

	return vmm_share_create_step(task, (struct vmm_shared_params*)task->io_finished.params[0], ioret);
}

/*
Invoked when a swap read has completed.
*/
UINT32 vmm_share_create_swp_callback(struct pm_task *task, UINT32 ioret)
{
	return vmm_share_create_step(task, (struct vmm_shared_params*)task->swp_io_finished.params[0], ioret);
}


/*
Map a memory range on a task to other task shared area.
RETURNS: TRUE if mapping begun.
Invokes command_inf callback upon completion.
*/
BOOL vmm_share_map(UINT16 descriptor_id, struct pm_task *task, ADDR laddr, UINT32 length, UINT16 perms)
{
	struct vmm_memory_region *mregit = NULL;
	UINT32 lstart, lend;
	struct vmm_shared_descriptor *desc = NULL;
	struct vmm_shared_params *params = NULL;
	struct pm_thread *thr = NULL;

	if(task->state != TSK_NORMAL) 
		return FALSE;

	/* Check address is not above max_addr */
	if(task->vmm_inf.max_addr <= (UINT32)laddr + length)
	{
		/* FIXME: Should send an exception signal... */
		return FALSE;
	}

	/* Check start is page aligned and length is divisible by 0x1000 */
	if((UINT32)laddr % 0x1000 != 0 || length % 0x1000 != 0) return FALSE;

	laddr = TRANSLATE_ADDR(laddr, ADDR);
	lstart = (UINT32)laddr;
	lend = ((UINT32)laddr + length);

	/* Get descriptor. */
	desc = (struct vmm_shared_descriptor*)vmm_regiondesc_get(descriptor_id, VMM_MEMREGION_SHARED);
	
	/* Check size is ok. */
	if((UINT32)desc->shared_laddr_end - (UINT32)desc->shared_laddr_start < length) return FALSE;
	
	if(desc == NULL) return FALSE;

	/* Check if a shared region already exists on the same task */
	mregit = task->vmm_inf.regions.first;

	while(mregit != NULL)
	{
		if( (lstart <= (UINT32)mregit->tsk_lstart && lend >= (UINT32)mregit->tsk_lstart) 
			|| (lstart <= (UINT32)mregit->tsk_lend && lend >= (UINT32)mregit->tsk_lstart) )
		{
			return FALSE;
		}

		mregit = mregit->next;
	}

	/* Task state will be set to TSK_MMAPPING */
	task->state = TSK_MMAPPING;

	/* Unschedule all threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	params = (struct vmm_shared_params*)kmalloc(sizeof(struct vmm_shared_params));

	if(params == NULL) return FALSE;

	/* Begin callback */
	params->params[0] = lstart;	    // param 0 will hold start address
	params->params[1] = lend;         // param 0 will hold ending address
	params->params[2] = (UINT32)desc;	// descriptor pointer
	params->params[3] = 0xFFFFFFFF;   // last swap address brought
	params->params[4] = lstart;	    // param 4 will hold start address
	params->params[5] = ((UINT32)perms & 0xFFFFFFFF);		// param 5 will contain permissions
	params->params[6] = 0xFFFFFFFF;	// last address paged in
	
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
	desc = (struct vmm_shared_descriptor*)params->params[2];

	ostart = (UINT32)desc->shared_laddr_start;
	oend = (UINT32)desc->shared_laddr_end;

	/* Free last swap address if not 0xFFFFFFFF */
	if(last_swp != 0xFFFFFFFF)
	{
		swap_free_addr(last_swp);

		/* Unset IOLOCK */
		vmm_set_flags(task->id, (ADDR)params->params[6], TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

		params->params[3] = 0xFFFFFFFF;
	}

	/* Check destination area is not swapped (if it is, I will free them no matter what is there, except for page tables) */
	pdir = task->vmm_inf.page_directory;

	for(; lstart < lend; lstart += 0x1000)
	{
		/* If page table is not present get us one */
		if(pdir->tables[PM_LINEAR_TO_DIR(lstart)].ia32entry.present == 0)
		{
			/* Get a fresh page */
			pg_addr = (UINT32)vmm_get_tblpage(task->id, lstart);

			task->vmm_inf.page_count++;
		
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

			task->vmm_inf.page_count--;
		}
	}

	lstart = params->params[4];	
	
	/* Allocate memory region */
	mreg = (struct vmm_memory_region*)kmalloc(sizeof(struct vmm_memory_region));

	if(mreg == NULL) return FALSE;

	otask = tsk_get(desc->owner_task);
	opdir = otask->vmm_inf.page_directory;

	/* 
	Map pages onto task 
	NOTE: All pages have already been paged out from task, and page tables 
	released from swap or allocated.
	NOTE2: Here I assume all pages on sharing task are paged in, if not... god help us.
	*/
	for(; lstart < lend; lstart += 0x1000)
	{
		optbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(opdir->tables[PM_LINEAR_TO_DIR(ostart)].b));

		/* Set the same page on page table */
		pm_page_in(task->id, (ADDR)lstart, (ADDR)optbl->pages[PM_LINEAR_TO_TAB(ostart)].entry.phy_page_addr, 2, ((task->swp_io_finished.params[5] & VMM_MEM_REGION_FLAG_WRITE)? PGATT_WRITE_ENA : 0));

		ostart += 0x1000;
	}

	/* Setup mreg */
	mreg->descriptor = desc;
	mreg->type = VMM_MEMREGION_SHARED;
	mreg->tsk_lstart = (ADDR)params->params[4];
	mreg->tsk_lend = (ADDR)lend;
	mreg->flags = params->params[5]; 

	/* Add region to the task */
	vmm_region_add(task, mreg);

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
	if(task->command_inf.callback != NULL)
			task->command_inf.callback(task, IO_RET_OK);

	kfree(params);

	return 1; // No errors, no Swap
}



