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
#include "types.h"
#include "task_thread.h"
#include "vm.h"
#include "taken.h"
#include "kmalloc.h"
#include "exception.h"
#include <services/pmanager/services.h>
#include "vmm_memarea.h"

/* Callback Declarations */
INT32 fmap_takeover_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_flush_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_flush_seek_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_realeased_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_flushed_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_seek_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_read_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 vmm_fmap_closed_callback(struct pm_task *task, INT32 ioret);

/* Functions Implementation */
BOOL vmm_page_filemapped(struct pm_task *task, struct pm_thread *thread, ADDR page_laddr)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_fmap_descriptor *fmd = NULL;

	/* If a memory region with type VMM_MEMREGION_FMAP exists, return TRUE and begin IO */
	if(task->vmm_info.regions == NULL) return FALSE;
    
    ma_node *n = ma_search_point(task->vmm_info.regions, UINT32 page_laddr);

	if(n == NULL) return FALSE;

    mreg = VMM_MEMREG_ADDRNODE2REG(n);

	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

	if(fmd->status != VMM_FMAP_ACTIVE) return FALSE;

	thread->vmm_info.fault_region = mreg;
	thread->vmm_info.page_perms = vmm_region_pageperms(mreg);	

	/* Block thread */
    sch_deactivate(thread);

	thread->state = THR_BLOCKED;
	thread->flags |= THR_FLAG_PAGEFAULT;
	thread->vmm_info.fault_address = page_laddr;
	thread->vmm_info.read_size = 0;
	thread->vmm_info.page_displacement = 0;
	
	/* IO lock Page table */
	vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(task->id, page_laddr)), TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

	thread->vmm_info.fmap_iosrc = fmd->iosrc;
	thread->vmm_info.fmap_iosrc.type = thread->io_event_src.type;
	thread->vmm_info.fmap_iosrc.id = thread->io_event_src.id;
    
    thread->vmm_info.pg_node.value = PG_ADDRESS(page_laddr);

    // we must insert the thread on the pg wait tree
    // in case another thread asks for the same page.
    rb_insert(&task->vmm_info.wait_root, &thread->vmm_info.pg_node, FALSE);
		
	/* Seek to the page possition */
	thread->io_finished.callback = vmm_fmap_seek_callback;
	io_begin_seek(&thread->vmm_info.fmap_iosrc, fmd->offset + (UINT32)mreg->tsk_node.low - (UINT32)page_laddr);

	return TRUE;
}

UINT32 vmm_fmap(UINT16 task_id, UINT32 fileid, UINT16 fs_task, ADDR start, UINT32 size, UINT32 perms, UINT32 offset)
{
	struct pm_task *task = tsk_get(task_id);
	struct vmm_fmap_descriptor *new_fm = NULL;
	struct vmm_memory_region *new_mreg = NULL, *mreg = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	UINT32 dindex, tindex, tindex_max, pg_addr;
	struct pm_thread *thr = NULL;

    if(task == NULL) return PM_TASK_ID_INVALID;

	/* On this implementation we will require start to be page aligned along with start + size */
	if((UINT32)start % 0x1000 != 0 || ((UINT32)start + size) % 0x1000 != 0)
		return PM_INVALID_PARAMS;

	/* Check address is not above max_addr */
	if(task->vmm_info.max_addr <= (UINT32)start + size)
	{
		return PM_ERROR;
	}

    /* Translate address adding base. */
	start = TRANSLATE_ADDR(start, ADDR);

    // check lstart and lend are "safe" to map
    if(loader_collides(task, start, start+size))
        return FALSE;

	/* Allocate structures */
	new_fm = kmalloc(sizeof(struct vmm_fmap_descriptor));   // descriptor

	if(new_fm == NULL) return PM_NOT_ENOUGH_MEM;

	new_mreg = kmalloc(sizeof(struct vmm_memory_region));   // memory region on the task

	if(new_mreg== NULL)
	{
		kfree(new_fm);
		return PM_NOT_ENOUGH_MEM;
	}

    // get an ID for the memory descriptor on the task 
	if(!rb_free_value(&task->regions_id, &new_mreg->tsk_id_node.value))
	{
		kfree(new_fm);
		kfree(new_mreg);
		return PM_TOO_MANY_FILES_OPENED;
	}

	io_init_source(&new_fm->iosrc, FILE_IO_FMAP, new_mreg->tsk_id_node.value);
	
	/* Initialize structures */
	new_fm->io_finished.callback = NULL;
	new_fm->iosrc.fs_service = fs_task;
	new_fm->iosrc.smo = -1;
	new_fm->status = VMM_FMAP_ACTIVE;
	new_fm->offset = offset;
	new_fm->next = NULL;
	new_fm->creating_task = task->id;
	new_fm->references = 1;	// This should change when we can share files mapped between tasks.
    new_fm->regions = new_mreg;
	
    new_mreg->next = new_mreg->prev = NULL;
	new_mreg->tsk_node.high = (ADDR)((UINT32)start + size);
	new_mreg->tsk_node.low = start;
	new_mreg->flags = perms;
	new_mreg->next = NULL;
	new_mreg->type = VMM_MEMREGION_FMAP;
    new_mreg->descriptor = new_fm;

	/* Check region does not intersect with other region on this task address space. */
	ma_node *n = ma_collition(&task->regions, start, start + size);

    if(n)
    {
        kfree(new_fm);
		kfree(new_mreg);
		return PM_MEM_COLITION;
    }

	/* 
	Check pages are not swapped.
	If pages are paged in, I'll page them out so an exception is raised if reading/writing.
	*/
	pdir = task->vmm_info.page_directory;
	ptbl = NULL;

	dindex = PM_LINEAR_TO_DIR(new_mreg->tsk_node.low);
	
	for(; dindex < PM_LINEAR_TO_DIR(new_mreg->tsk_node.high); dindex++)
	{
		if(pdir->tables[dindex].ia32entry.present == 0 && pdir->tables[dindex].record.swapped == 1)
		{
			kfree(new_fm);
			kfree(new_mreg);
			return PM_NOT_ENOUGH_MEM;
		}

		if(pdir->tables[dindex].ia32entry.present == 1)
		{
			/* Table is present check pages */
			if(dindex == PM_LINEAR_TO_DIR(new_mreg->tsk_node.low))
				tindex = PM_LINEAR_TO_TAB(new_mreg->tsk_node.low);
			else
				tindex = 0;

			if(dindex == PM_LINEAR_TO_DIR(new_mreg->tsk_node.high))
				tindex_max = PM_LINEAR_TO_TAB(new_mreg->tsk_node.high);
			else
				tindex_max = 1024;

			for(;tindex < tindex_max; tindex++)
			{
				if(ptbl->pages[tindex].entry.ia32entry.present == 0 && ptbl->pages[tindex].entry.record.swapped == 1)
				{
					kfree(new_fm);
					kfree(new_mreg);
					return PM_NOT_ENOUGH_MEM;
				}
				else if(ptbl->pages[tindex].entry.ia32entry.present == 1)
				{
					pg_addr = PHYSICAL2LINEAR(PG_ADDRESS(ptbl->pages[tindex].entry.phy_page_addr));

					/* Page will be paged out */
					page_out(task->id, (ADDR)(dindex * 0x400000 + tindex * 0x1000), 2);
					
					/* Put page onto PMAN */
					vmm_put_page((ADDR)pg_addr);
				}
			}
		}
	}

    /* Add new_mreg on the task lists */
    ma_insert(&task->regions, &new_mreg->tsk_node);
    rb_search(&task->regions_id, &new_mreg->tsk_id_node);

	/* Once takeover is OK, new_fm will be addede to the global list. 
    I can't add it now, because I don't have a */

	/* Tell stdfss we are taking control of the file. */
    new_fm->io_finished.params[0] = task->id;
	new_fm->io_finished.callback  = fmap_takeover_callback;
	io_begin_takeover(&new_fm->iosrc, fileid, new_fm, task->id);

	/* Block all threads for this task, until the file is opened so we don't have page faults */
	task->state = TSK_MMAPPING;
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	return PM_OK;
}

BOOL vmm_fmap_release_mreg(struct pm_task *task, struct vmm_memory_region *mreg);

/* Release an FMAP */
BOOL vmm_fmap_release(struct pm_task *task, ADDR lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct pm_thread *thread = NULL;
	struct vmm_fmap_descriptor *fmd = NULL;
	UINT32 addr;

	/* Find our mapping */
	ma_node *n = ma_search_point(&task->regions, (UINT32)lstart);

	if(n == NULL) return FALSE;

    mreg = VMM_MEMREG_MEMA2MEMR(n);

	/* If we have a thread waiting for this FMAP, fail */
	thread = task->first_thread;

	while(thread != NULL)
	{
		if(thread->vmm_info.fault_region == mreg) 
            return FALSE;
		thread = thread->next_thread;
	}

    vmm_fmap_release_mreg(task, mreg, 0);
}

BOOL vmm_fmap_release_mreg(struct pm_task *task, struct vmm_memory_region *mreg)
{
	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

    if(fmd->status == VMM_FMAP_FLUSHING) // since we only allow one command on the task command 
                                         // queue, there cannot be a flush command executing.. 
                                         // then. the task is dying.
    {
        // fmap is flushing.. we must wait until it completes
        // but let's tell the current action we intend to close the FMAP
        tmd->status = VMM_FMAP_CLOSING_RELEASE;
        return FALSE;
    }
	else if(fmd->status != VMM_FMAP_ACTIVE)
    {
        return FALSE;
    }

	/* Begin removal only if references is 1 */
	fmd->references--;

	if(fmd->references > 0)
	{
        // this fmap still has references from other task

		/* Unmap Pages from task address space */
		addr = (UINT32)mreg->tsk_node.low;

		for(; addr < (UINT32)mreg->tsk_node.high; addr += 0x1000)
		{
			page_out(task->id, (ADDR)addr, 2);
		}

		/* Remove memory region from trees */
		ma_remove(&task->regions, &mreg->tsk_node);
        rb_remove(&task->regions_id, &mreg->tsk_id_node);

        fmd->region = NULL;
		kfree(mreg);

		return TRUE;
	}

	/* Set FMAP as shutting down */
	fmd->status = VMM_FMAP_CLOSING;

	/* Write all dirty pages */
	fmd->io_finished.params[0] = 0;	// 0 will indicate we are closing and not flushing
	fmd->io_finished.callback = vmm_fmap_flush_callback;

	fmd->release_addr = (UINT32)mreg->tsk_node.low - 0x1000;
	fmd->creating_task = task->id;

	vmm_fmap_flush_callback(&fmd->iosrc, IO_RET_OK);

	return TRUE;
}

/*
This function will flush to IO device modified pages
*/
BOOL vmm_fmap_flush(struct pm_task *task, ADDR tsk_node.low)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_fmap_descriptor *fmd = NULL;

	/* Find our mapping */
	ma_node *n = ma_search_point(&task->regions, (UINT32)tsk_node.low);

	if(n == NULL) return FALSE;

    mreg = VMM_MEMREG_MEMA2MEMR(n);

	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

    if(fmd->status != VMM_FMAP_ACTIVE)
    {
        return FALSE;
    }

	/* Set FMAP as flushing */
	fmd->status = VMM_FMAP_FLUSHING;

	/* Write all dirty pages */
	fmd->io_finished.params[0] = 1;	// 1 will indicate we are flushing
	fmd->io_finished.callback = vmm_fmap_flush_callback;

	fmd->release_addr = (UINT32)mreg->tsk_node.low - 0x1000;
	fmd->creating_task = task->id;

	/* Invoke flush callback */
	vmm_fmap_flush_callback(&fmd->iosrc, IO_RET_OK);

	return TRUE;
}

/* Release the specified fmap */
void vmm_fmap_task_closing(struct pm_task *task, struct vmm_memory_region *mreg)
{	
    task->command_inf.callback = vmm_fmap_closed_callback;
    vmm_fmap_release_mreg(task, mreg, 1);
}

/* Callback invoked when closing an fmap because the task is being killed */
INT32 vmm_fmap_closed_callback(struct pm_task *task, INT32 ioret)
{
    // continue closing the task
    vmm_close_task(task);
}

/*
Function invoked when the file has been taken over for mapping.
NOTE: IO src belongs to fmap descriptor!
*/
INT32 fmap_takeover_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = NULL;
	struct vmm_fmap_descriptor *fm;
	struct vmm_memory_region *mreg;
	struct pm_thread *thr = NULL;

    task = tsk_get(new_fm->io_finished.params[0]);  // params[0] has the task id

	if(task == NULL) return 0;

    rbnode *n = rb_search(&task->regions_id, iosrc->id);

    if(!n) return 0;

    fm = IDNODE2FMAPDESC(n);
    	
	/* If file open succeded send a successful response. */
	if(ioret == IO_RET_ERR)
	{
        /* Takeover failed, remove the memory region from it's trees */
        ma_remove(&task->regions, &fm->regions->tsk_node);
        rb_remove(&task->regions_id, &fm->regions->tsk_id_node);

		kfree(fm);
		kfree(mreg);
	}	
	else
	{
        /* Get our file id for this file */
		fm->iosrc.file_id = fm->gnode.value = task->io_finished.params[0];

        /* Add the FMAP descriptor to the global list */
        rb_insert(&vmm.fmap_descriptors, &fm->gnode);
	}

	/* Activate task threads again. */
    task->state = TSK_NORMAL;
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_deactivate(thr);
		thr = thr->next_thread;
	}

	if(task->command_inf.callback != NULL) 
		task->command_inf.callback(task, ioret);

	return 1;
}
/* 
Invoked when releasing an FMAP page.
IO src belongs to an FMAP descriptor.
*/
INT32 vmm_fmap_flush_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct vmm_fmap_descriptor *fm = MEMREGNODE2FMAPDESC(rb_search(&vmm->fmap_descriptors, iosrc->id));
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_page_directory *pdir = task->vmm_info.page_directory;
	struct vmm_page_table *tbl = NULL;
	struct vmm_memory_region *mreg = vmm_region_get_bydesc((struct vmm_descriptor*)fm);
	struct vmm_pman_assigned_record *assigned = NULL;

    if(task == NULL) return 0;

	if(ioret == IO_RET_OK)
	{
		/* Remove IOLOCK */
		if(fm->release_addr >= (UINT32)mreg->tsk_node.low) // the first time this function is invoked this condition won't hold
		{
			tbl = (struct vmm_page_table *)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_TAB(fm->release_addr)].b));
			
			/* If we are flushing, we won't return the page to PMAN */
			if(fm->io_finished.params[0] == 1)
			{
				/* Restore assigned record */
				assigned = vmm_get_assigned(task->id, fm->release_addr);
				*assigned = *((struct vmm_pman_assigned_record*)&fm->io_finished.params[1]);

				/* Remove IOLOCK */
				vmm_set_flags(fm->creating_task, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)), TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
			}
			else
			{
				/* Put page onto vmm */
				vmm_put_page((ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)));

				/* NOTE: I don't remove IO lock here for taken record has been reseted with vmm_put_page */
			}			
		}

        // if there is an FMAP release pending, let's finish the 
        // flush early.. FMAP will be released and all remaining
        // dirty pages will be written
        if(fm->state == VMM_FMAP_CLOSING_RELEASE)
        {
            vmm_fmap_task_closing(task, mreg);
            return 0;        
        }

		fm->release_addr += 0x1000;
		
		while((UINT32)mreg->tsk_node.high != fm->release_addr)
		{
			/* Find first present table */
			pdir = task->vmm_info.page_directory;
			while(pdir->tables[PM_LINEAR_TO_DIR(fm->release_addr)].ia32entry.present != 1 
                && (UINT32)mreg->tsk_node.high != fm->release_addr)
			{
				fm->release_addr += 0x1000;
			}

			if((UINT32)mreg->tsk_node.high == fm->release_addr)
			{
				/* Finished */
				if(fm->io_finished.params[0] == 1)
				{
					return vmm_fmap_flushed_callback(&fm->iosrc, IO_RET_OK);
				}
				else
				{
					fm->io_finished.callback = vmm_fmap_realeased_callback;
					io_begin_release(&fm->iosrc);
				}
				
				return 0;
			}

			tbl = (struct vmm_page_table *)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_TAB(fm->release_addr)].b));
			
            /* Find first present page */
			while(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.ia32entry.present != 1 
                && (UINT32)mreg->tsk_node.high != fm->release_addr)
			{
				fm->release_addr += 0x1000;
			}
			
			if((UINT32)mreg->tsk_node.high == fm->release_addr)
			{
				/* Finished */
				if(fm->io_finished.params[0] == 1)
				{
					return vmm_fmap_flushed_callback(&fm->iosrc, IO_RET_OK);
				}
				else
				{
					fm->io_finished.callback = vmm_fmap_realeased_callback;
					io_begin_release(&fm->iosrc);
				}
				return 0;
			}

			/* Got one present page, see if it's dirty */
			if(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.ia32entry.dirty == 1)
			{
				/* IOLOCK */
				vmm_set_flags(fm->creating_task, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)), TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

				/* Seek */
				fm->io_finished.callback = vmm_fmap_flush_seek_callback;
				io_begin_seek(&fm->iosrc, (fm->release_addr - (UINT32)mreg->tsk_node.low) + fm->offset);
				return 1;
			}
		}
		
		/* Finished releasing  */
		if(fm->io_finished.params[0] == 1)
		{
			return vmm_fmap_flushed_callback(&fm->iosrc, IO_RET_OK);
		}
		else
		{
			fm->io_finished.callback = vmm_fmap_realeased_callback;
			io_begin_release(&fm->iosrc);
		}
	}
	else
	{
        if(fm->state == VMM_FMAP_CLOSING_RELEASE)
        {
            vmm_fmap_task_closing(task, mreg);
            return 0;        
        }

		/* Failed */
        if(task->command_inf.callback != NULL) 
	        task->command_inf.callback(task, ioret);
	}
	return 0;
}

INT32 vmm_fmap_flush_seek_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct vmm_fmap_descriptor *fm = MEMREGNODE2FMAPDESC(rb_search(&vmm->fmap_descriptors, iosrc->id));
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_memory_region *mreg = vmm_region_get_bydesc((struct vmm_descriptor*)fm);
	struct vmm_page_directory *pdir = task->vmm_info.page_directory;
	struct vmm_page_table *tbl = NULL;
	struct vmm_pman_assigned_record *assigned = NULL;

    if(task == NULL) return 0;

	tbl = (struct vmm_page_table *)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_TAB(fm->release_addr)].b));
		
	if(ioret == IO_RET_OK)
	{
		/* If flushing save assigned record */
		if(fm->io_finished.params[0] == 1)
		{
			assigned = vmm_get_assigned(task->id, fm->release_addr);
			fm->io_finished.params[1] = *((UINT32*)assigned);
		}

		/* 
		Map page onto pman space (WARNING: We are loosing assigned record here... but 
		it doesn´t matter for we are not swapping out this pages)
		*/
		page_in(PMAN_TASK, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)), 
                (ADDR)tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr, 2, PGATT_WRITE_ENA);

		/* Write and set callback to continue. */
		fm->io_finished.callback = vmm_fmap_flush_callback;
		io_begin_write(&fm->iosrc, 0x1000, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)));
		
		return 1;
	}
	else
	{
		/* Remove IOLOCK */
		vmm_set_flags(fm->creating_task, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)), TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

		/* Failed */
		if(task->command_inf.callback != NULL) 
			task->command_inf.callback(task, ioret);
	}
	return 0;
}

INT32 vmm_fmap_realeased_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct vmm_fmap_descriptor *fm = MEMREGNODE2FMAPDESC(rb_search(&vmm->fmap_descriptors, iosrc->id));
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_memory_region *mreg = vmm_region_get_bydesc((struct vmm_descriptor*)fm);

    if(task == NULL) return 0;

	if(ioret == IO_RET_OK)
	{
		/* Remove memory region from trees */
		ma_remove(&task->regions, &mreg->tsk_node);
        rb_remove(&task->regions_id, &mreg->tsk_id_node);

        fmd->region = NULL;
		kfree(mreg);

		/* Remove FMAP descriptor from global list */
		rb_remove(&vmm.fmap_descriptors, &fm.gnode);

		/* Free structures */
		kfree(fm);
		kfree(mreg);

		/* Invoke command module callback */
		if(task->command_inf.callback != NULL) 
			task->command_inf.callback(task, ioret);

		return 1;
	}
	else
	{
		/* Failed */
		if(task->command_inf.callback != NULL) 
			task->command_inf.callback(task, ioret);

		return 0;
	}
}

/*
Invoked when flushing is complete.
*/
INT32 vmm_fmap_flushed_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct vmm_fmap_descriptor *fm = MEMREGNODE2FMAPDESC(rb_search(&vmm->fmap_descriptors, iosrc->id));
	struct pm_task *task = tsk_get(fm->creating_task);

    if(task == NULL) return 0;

	if(task->command_inf.callback != NULL) 
			task->command_inf.callback(task, ioret);

	return 1;
}

/*
Invoked by a thread fmap IO src upon seek completion.
*/
INT32 vmm_fmap_seek_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_thread *thr = thr_get(iosrc->id);
	struct vmm_memory_region *mreg = thr->vmm_info.fault_region;
	struct pm_task *task = tsk_get(thr->task_id);
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)mreg->descriptor;

    if(task == NULL) return 0;

	if(ioret == IO_RET_OK)
	{
		/* Seek completed, get a free page and begin IO */
		thr->vmm_info.page_in_address = vmm_get_page(task->id, PG_ADDRESS(thr->vmm_info.fault_address));
		
		if(thr->vmm_info.page_in_address == NULL)
		{
			fatal_exception(task->id, NOT_ENOUGH_MEMORY);
			return 0;
		}

		task->vmm_info.page_count++;

		/* Set FILE flag on page */
		vmm_set_flags(task->id, thr->vmm_info.page_in_address, FALSE, TAKEN_PG_FLAG_FILE, TRUE);

		thr->io_finished.callback = vmm_fmap_read_callback;
		io_begin_read(&thr->vmm_info.fmap_iosrc, 0x1000, thr->vmm_info.page_in_address);
	}
	else
	{
		/* Could not seek */
        thr->vmm_info.page_in_address = NULL;
		vmm_page_ioerror(thr, FALSE);
		return 0;
	}

	return 1;
}

/*
Invoked by a thread IO src upon read completion.
*/
INT32 vmm_fmap_read_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_thread *thread = thr_get(iosrc->id), *thr = NULL;
	struct pm_task *task = tsk_get(thread->task_id);
	struct taken_entry *t = NULL;

    if(task == NULL) return 0;

	/* Unblock task threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_activate(thr);
		thr = thr->next_thread;
	}

	if(ioret == IO_RET_OK)
	{
        // check if any threads where killed while waiting for this page
        vmm_check_threads_pg(&thread, FALSE);

        if(thread == NULL) 
            return 0; // no threads left!

		/* Page in on process address space */
		pm_page_in(thread->task_id, thread->vmm_info.fault_address, (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);

		/* Un set IOLCK and PF eflags on the page */
		vmm_set_flags(thread->task_id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK, FALSE);

		/* Remove page from pman address space and create assigned record. */
		vmm_unmap_page(thread->task_id, PG_ADDRESS(thread->vmm_info.fault_address));

		/* Set taken record */
		t = vmm_taken_get((ADDR)PG_ADDRESS(thread->vmm_info.page_in_address));

		t->data.b_pg.tbl_index = iosrc->id;

		/* Wake threads waiting for this page */
		vmm_wake_pf_threads(thread);
	}
	else
	{
        vmm_page_ioerror(thread, FALSE);
		return 0;
	}
	return 1;
}


