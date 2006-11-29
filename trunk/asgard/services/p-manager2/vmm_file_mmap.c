
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

/*
THERE'S STILL AN OPEN ISSUE HERE (well a lot because this sucks)
IF A TASK IS KILLED WHILE AN FMAP READ / WRITE OP IS BEING 
PERFORMED RESPONSE MESSAGES MIGHT CAUSE A MESS. WE SHOULD DO
SOMETHING ABOUT IT, LIKE PUTTING THE KILL ON HOLD.
*/

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
	if(task->vmm_inf.regions.total == 0) return FALSE;

	mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if((UINT32)mreg->tsk_lstart <= (UINT32)page_laddr && (UINT32)page_laddr < (UINT32)mreg->tsk_lend && mreg->type == VMM_MEMREGION_FMAP)
			break;
		
		mreg = mreg->next;
	}

	if(mreg == NULL) return FALSE;

	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

	if(fmd->status != VMM_FMAP_ACTIVE) return FALSE;

	thread->vmm_info.fault_region = mreg;
	thread->vmm_info.page_perms = vmm_region_pageperms(mreg);	

	/* Block thread */
	thread->state = THR_BLOCKED;
	thread->flags |= THR_FLAG_PAGEFAULT;
	thread->vmm_info.fault_address = page_laddr;
	thread->vmm_info.read_size = 0;
	thread->vmm_info.page_displacement = 0;
	thread->vmm_info.fault_next_thread = NULL;
	thread->vmm_info.swaptbl_next = NULL;

	/* IO lock Page table */
	vmm_set_flags(task->id, (ADDR)PHYSICAL2LINEAR(vmm_get_tbl_physical(task->id, page_laddr)), TRUE, TAKEN_EFLAG_IOLOCK, TRUE);

	thread->vmm_info.fmap_iosrc = fmd->iosrc;
	thread->vmm_info.fmap_iosrc.type = thread->io_event_src.type;
	thread->vmm_info.fmap_iosrc.id = thread->io_event_src.id;

	/* Seek to the page possition */
	thread->io_finished.callback = vmm_fmap_seek_callback;
	io_begin_seek(&thread->vmm_info.fmap_iosrc, fmd->offset + (UINT32)mreg->tsk_lstart - (UINT32)page_laddr);

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

	/* On this implementation we will require start to be page aligned along with start + size */
	if((UINT32)start % 0x1000 != 0 || ((UINT32)start + size) % 0x1000 != 0)
		return PM_INVALID_PARAMS;

	/* Check address is not above max_addr */
	if(task->vmm_inf.max_addr <= (UINT32)start + size)
	{
		/* FIXME: Should send an exception signal... */
		return FALSE;
	}

	/* Translate address adding base. */
	start = TRANSLATE_ADDR(start, ADDR);

	/* Allocate structures */
	new_fm = kmalloc(sizeof(struct vmm_fmap_descriptor));

	if(new_fm == NULL) return PM_NOT_ENOUGH_MEM;

	new_mreg = kmalloc(sizeof(struct vmm_memory_region));

	if(new_mreg== NULL)
	{
		kfree(new_fm);
		return PM_NOT_ENOUGH_MEM;
	}

	new_fm->id = vmm_regiondesc_getid();

	if(new_fm->id == 0xFFFF)
	{
		kfree(new_fm);
		kfree(new_mreg);
		return PM_TOO_MANY_FILES_OPENED;
	}

	io_init_source(&new_fm->iosrc, FILE_IO_FMAP, new_fm->id);
	
	/* Initialize structures */
	new_fm->io_finished.callback = NULL;
	new_fm->iosrc.fs_service = fs_task;
	new_fm->iosrc.smo = -1;
	new_fm->status = VMM_FMAP_ACTIVE;
	new_fm->offset = offset;
	new_fm->next = NULL;
	new_fm->creating_task = task->id;
	new_fm->references = 1;	// This should change when we can share files mapped between tasks.
	
	new_mreg->tsk_lend = (ADDR)((UINT32)start + size);
	new_mreg->tsk_lstart = start;
	new_mreg->flags = perms;
	new_mreg->next = NULL;
	new_mreg->type = VMM_MEMREGION_FMAP;

	/* Check region does not intersect with other region on this task address space. */
	mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if( (new_mreg->tsk_lstart <= mreg->tsk_lstart && new_mreg->tsk_lend > mreg->tsk_lstart) 
			|| (new_mreg->tsk_lstart <= mreg->tsk_lend && new_mreg->tsk_lend > mreg->tsk_lstart) )
		{
			kfree(new_fm);
			kfree(new_mreg);
			return PM_MEM_COLITION;
		}
		mreg = mreg->next;
	}

	/* 
	Check pages are not swapped.
	If pages are paged in, I'll page them out so an exception is raised if reading/writing.
	*/
	pdir = task->vmm_inf.page_directory;
	ptbl = NULL;

	dindex = PM_LINEAR_TO_DIR(new_mreg->tsk_lstart);
	tindex = 0;

	for(; dindex < PM_LINEAR_TO_DIR(new_mreg->tsk_lend); dindex++)
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
			if(dindex == PM_LINEAR_TO_DIR(new_mreg->tsk_lstart))
				tindex = PM_LINEAR_TO_TAB(new_mreg->tsk_lstart);
			else
				tindex = 0;

			if(dindex == PM_LINEAR_TO_DIR(new_mreg->tsk_lend))
				tindex_max = PM_LINEAR_TO_TAB(new_mreg->tsk_lend);
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

	/* Add region descriptor to global list */
	vmm_regiondesc_add((struct vmm_region_descriptor*)new_fm);

	/* Add region to task regions list */
	vmm_region_add(task, new_mreg);
	
	/* Tell stdfss we are taking control of the file. */
	new_fm->io_finished.callback  = fmap_takeover_callback;
	io_begin_takeover(&new_fm->iosrc, fileid, task->id);

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

/* Release an FMAP */
BOOL vmm_fmap_release(struct pm_task *task, ADDR tsk_lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct pm_thread *thread = NULL;
	struct vmm_fmap_descriptor *fmd = NULL;
	UINT32 addr;

	/* Find our mapping */
	mreg = vmm_region_get(task, (UINT32)tsk_lstart);

	if(mreg == NULL) return FALSE;

	/* If we have a thread waiting for this FMAP, fail */
	thread = task->first_thread;

	while(thread != NULL)
	{
		if(thread->vmm_info.fault_region == mreg) return FALSE;
		thread = thread->next_thread;
	}

	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

	if(fmd->status != VMM_FMAP_ACTIVE) return FALSE;

	/* Begin removal only if references is 1 */
	fmd->references--;

	if(fmd->references > 0)
	{
		/* Unmap Pages from task address space */
		addr = (UINT32)mreg->tsk_lstart;

		for(; addr < (UINT32)mreg->tsk_lend; addr += 0x1000)
		{
			page_out(task->id, (ADDR)addr, 2);
		}

		/* Remove memory region. */
		vmm_region_remove(task, mreg);

		kfree(mreg);

		return TRUE;
	}

	/* Set FMAP as shutting down */
	fmd->status = VMM_FMAP_CLOSING;

	/* Write all dirty pages */
	fmd->io_finished.params[0] = 0;	// 0 will indicate we are closing and not flushing
	fmd->io_finished.callback = vmm_fmap_flush_callback;

	fmd->release_addr = (UINT32)mreg->tsk_lstart - 0x1000;
	fmd->creating_task = task->id;

	vmm_fmap_flush_callback(&fmd->iosrc, IO_RET_OK);

	return TRUE;
}

/*
This function will flush to IO device modified pages
*/
BOOL vmm_fmap_flush(struct pm_task *task, ADDR tsk_lstart)
{
	struct vmm_memory_region *mreg = NULL;
	struct vmm_fmap_descriptor *fmd = NULL;

	/* Find our mapping */
	mreg = vmm_region_get(task, (UINT32)tsk_lstart);

	if(mreg == NULL) return FALSE;

	fmd = (struct vmm_fmap_descriptor*)mreg->descriptor;

	if(fmd->status != VMM_FMAP_ACTIVE) return FALSE;

	/* Set FMAP as shutting down */
	fmd->status = VMM_FMAP_CLOSING;

	/* Write all dirty pages */
	fmd->io_finished.params[0] = 1;	// 1 will indicate we are flushing
	fmd->io_finished.callback = vmm_fmap_flush_callback;

	fmd->release_addr = (UINT32)mreg->tsk_lstart - 0x1000;
	fmd->creating_task = task->id;

	/* Invoke flush callback */
	vmm_fmap_flush_callback(&fmd->iosrc, IO_RET_OK);

	return TRUE;
}

/*
Function invoked when the file has been taken over for mapping.
NOTE: IO src belongs to fmap descriptor!
*/
INT32 fmap_takeover_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = NULL;
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
	struct vmm_memory_region *mreg;
	struct pm_thread *thr = NULL;

	if(fm == NULL) return 0;

	task = tsk_get(fm->creating_task);

	if(task == NULL) return 0;

	/* If file open succeded send a successful response. */
	if(ioret == IO_RET_ERR)
	{
		mreg = vmm_region_get_bydesc(task, fm);

		/* File open failed, descriptor and region */
		vmm_region_remove(task,mreg);
		vmm_regiondesc_remove(fm);

		/* Remove from global list */
		kfree(fm);
		kfree(mreg);
		
		fm = NULL;

		/* Reactivate threads */
		task->state = TSK_NORMAL;
		thr = task->first_thread;

		while(thr != NULL)
		{
			sch_activate(thr);
			thr = thr->next_thread;			
		}
	}	
	else
	{
		/* Get our file id for this file */
		fm->iosrc.file_id = task->io_finished.params[0];
	}

	/* Activate task threads again. */
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
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;
	struct vmm_page_table *tbl = NULL;
	struct vmm_memory_region *mreg = vmm_region_get_bydesc(task, fm);
	struct vmm_pman_assigned_record *assigned = NULL;

	if(ioret == IO_RET_OK)
	{
		/* Remove IOLOCK */
		if(fm->release_addr >= (UINT32)mreg->tsk_lstart) 
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

		fm->release_addr += 0x1000;
		
		while((UINT32)mreg->tsk_lend != fm->release_addr)
		{
			/* Check page is present */
			pdir = task->vmm_inf.page_directory;
			while(pdir->tables[PM_LINEAR_TO_DIR(fm->release_addr)].ia32entry.present != 1 && (UINT32)mreg->tsk_lend != fm->release_addr)
			{
				fm->release_addr += 0x1000;
			}

			if((UINT32)mreg->tsk_lend == fm->release_addr)
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
			
			while(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.ia32entry.present != 1 && (UINT32)mreg->tsk_lend != fm->release_addr)
			{
				fm->release_addr += 0x1000;
			}
			
			if((UINT32)mreg->tsk_lend == fm->release_addr)
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
				io_begin_seek(&fm->iosrc, (fm->release_addr - (UINT32)mreg->tsk_lstart) + fm->offset);
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
		/* Failed */
		if(task->command_inf.callback != NULL) 
			task->command_inf.callback(task, ioret);
	}
	return 0;
}

INT32 vmm_fmap_flush_seek_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_memory_region *mreg = vmm_region_get_bydesc(task, fm);
	struct vmm_page_directory *pdir = task->vmm_inf.page_directory;
	struct vmm_page_table *tbl = NULL;
	struct vmm_pman_assigned_record *assigned = NULL;

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
		page_in(PMAN_TASK, (ADDR)PHYSICAL2LINEAR(PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr)), (ADDR)tbl->pages[PM_LINEAR_TO_TAB(fm->release_addr)].entry.phy_page_addr, 2, PGATT_WRITE_ENA);

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
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_memory_region *mreg = vmm_region_get_bydesc(task, fm);

	if(ioret == IO_RET_OK)
	{
		/* Remove memory region from task list */
		vmm_region_remove(task, mreg);

		/* Remove FMAP descriptor from global list */
		vmm_regiondesc_remove(fm);

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
	struct vmm_fmap_descriptor *fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
	struct pm_task *task = tsk_get(fm->creating_task);
	struct vmm_memory_region *mreg = vmm_region_get_bydesc(task, fm);

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

	if(ioret == IO_RET_OK)
	{
		/* Seek completed, get a free page and begin IO */
		thr->vmm_info.page_in_address = vmm_get_page(task->id, PG_ADDRESS(thr->vmm_info.fault_address));
		
		if(thr->vmm_info.page_in_address == NULL)
		{
			fatal_exception(task->id, NOT_ENOUGH_MEMORY);
			return 0;
		}

		task->vmm_inf.page_count++;

		/* Set FILE flag on page */
		vmm_set_flags(task->id, thr->vmm_info.page_in_address, FALSE, TAKEN_PG_FLAG_FILE, TRUE);

		thr->io_finished.callback = vmm_fmap_read_callback;
		io_begin_read(&thr->vmm_info.fmap_iosrc, 0x1000, thr->vmm_info.page_in_address);
	}
	else
	{
		/* Could not seek */
		fatal_exception(task->id, FMAP_IO_ERROR);
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

	/* Unblock task threads */
	thr = task->first_thread;

	while(thr != NULL)
	{
		sch_activate(thr);
		thr = thr->next_thread;
	}

	if(ioret == IO_RET_OK)
	{
		/* Page in on process address space */
		pm_page_in(thread->task_id, thread->vmm_info.fault_address, (ADDR)LINEAR2PHYSICAL(thread->vmm_info.page_in_address), 2, thread->vmm_info.page_perms);

		/* Un set IOLCK and PF eflags on the page */
		vmm_set_flags(thread->task_id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK, FALSE);
		
		task = tsk_get(thread->task_id);

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
		vmm_put_page(thread->vmm_info.page_in_address);

		/* Could not read */
		fatal_exception(task->id, FMAP_IO_ERROR);
		return 0;
	}
	return 1;
}

/* Begin closing File Mappings on the task. */
void vmm_fmap_close_all(struct pm_task *task)
{
	struct vmm_memory_region *mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		/* We will set command_inf callback to our function */
		task->command_inf.callback = vmm_fmap_closed_callback;

		/* Begin memory region releasing. */
		if((mreg->type == VMM_MEMREGION_FMAP) && vmm_fmap_release(task, mreg->tsk_lstart)) return;
		
		mreg = mreg->next;
	}

	/* If we got here, it's because there are no more FMAPs to free. */

	/* Here we must invoke the command callback */
	if(task->io_finished.callback != NULL) task->io_finished.callback( &task->io_event_src, IO_RET_OK);
}

/*
This callback will be invoked when finished releasing an FMAP upon task close.
*/
INT32 vmm_fmap_closed_callback(struct pm_task *task, INT32 ioret) 
{
	/* Do I really care if we finished ok? I don't think so... Invoke vmm_fmap_close_all again so it continues. */
	vmm_fmap_close_all(task);

	return 0;
}

