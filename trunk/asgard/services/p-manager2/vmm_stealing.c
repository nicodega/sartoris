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


#include "formats/ia32paging.h"
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "types.h"
#include "task_thread.h"
#include "vm.h"
#include "swap.h"
#include "io.h"
#include "interrupts.h"
#include "layout.h"

extern UINT32 curr_st_region;
extern UINT32 curr_ag_region;

#define ABS(a) ((a>0)? a : -1*a)

/*
Page Stealing Thread.
This thread will go through taken entries stealing pages.
*/
void vmm_page_stealer()
{
	UINT32 tindex, iteration, pages_required, pages_swapped;
	UINT32 candidate_points, candidate_io, candidate_addr, candidate_task_id, candidate_laddress;
	struct taken_entry *candidate_taken = NULL;
	struct taken_entry *entry = NULL;
	struct pm_task *task = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;
	struct io_slot *ioslot = NULL;
	struct taken_table *ttable;
	BOOL iorequired = FALSE, write;
	UINT16 task_id;
	UINT32 points, task_laddress, ioslot_id, swap_addr;

	/* Go through taken entries.*/
	int_set(0);

	/* Begin stealing loop. */
	for(;;)
	{
		tindex = 0;
		iteration = 0;			// current iteration		
		pages_required = 0;		// how many pages will we attempt to get from this region
		pages_swapped = 0;

		int_clear();
		if(vmm.available_mem < vmm.max_mem && vmm.available_mem > vmm.mid_mem)
		{
			pages_required = 0;
		}
		else if(vmm.available_mem < vmm.mid_mem && vmm.available_mem > vmm.min_mem)
		{
			/* We need quite a few. */
			pages_required = 10;
		}
		else if(vmm.available_mem < vmm.min_mem)
		{
			/* We are desperate for pages. */
			pages_required = 30;
		}
		int_set(0);

		/* Get our current region taken table. */
		ttable = vmm.taken.tables[curr_st_region];

		while(iteration < pages_required)
		{
			candidate_points = 0xFFFFFFFF;
			candidate_io = 0;
			candidate_addr;
			candidate_task_id;
			candidate_laddress;
			
			/* If we need memory */

			/* Cicle through taken entries on this region looking for the best candidate. */
			for(tindex = 0; tindex < 1024; tindex++)
			{
				entry = &ttable->entries[tindex];

				/* 
					NOTE: remember to cli/sti while inserting a page 
					on the free pool!! so pman won't interrupt 
					at the middle of the operation 
					NOTE2: Page faults are non maskable interrupts, but this implies no 
					problem for us if we cli when modifying the free_pages list
					for we won't page fault, and we will be the only thread running.
				*/
				
				int_clear();
				
				entry = &ttable->entries[tindex];

				/* If page is taken, it's not from a service, not a directory and not IOLOCKED */
				if(entry->data.b_pdir.taken == 1 && entry->data.b_pdir.dir == 0 && !(entry->data.b_pdir.eflags & (TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_SERVICE)))
				{
					/* If its not a page table, or its a page table and we *really* need memory. */
					if( (entry->data.b_pdir.tbl == 0) || ( pages_swapped < (pages_required >> 1) && pages_required > 30) )
					{
						/* Ignore pman pages.
                        Here I'm also ignoring file memory maps, but we should consider them and flush them if needed. */
						if(entry->data.b_pdir.tbl == 0 && (entry->data.b_pg.flags & (TAKEN_PG_FLAG_PMAN | TAKEN_PG_FLAG_PHYMAP | TAKEN_PG_FLAG_SHARED | TAKEN_PG_FLAG_FILE)))
						{
							int_set(0);
							continue;
						}

						/* Calculate page points */
						iorequired = FALSE;
						points = calculate_points(entry, curr_st_region, tindex, (ioslots.free_count > 0 && swap_get_addr() != 0xFFFFFFFF), &iorequired, &task_id, &task_laddress);

						if(points < candidate_points)
						{
							/* This page won, it'll be our new candidate */
							candidate_taken = entry;
							candidate_points = points;
							candidate_io = iorequired;
							candidate_addr = curr_st_region * 0x400000 + tindex * 0x1000;
							candidate_laddress = task_laddress;
							candidate_task_id = task_id;
						}
					}
				}
				int_set(0);		
			}

            if(candidate_points == 0xFFFFFFFF)
            {
                // there are no candidates here
                int_set(0);
                run_thread(SCHED_THR);  // run scheduler thread
			    continue;
            }

			if(candidate_io)
			{
				/* Candidate must be sent to swap file! */	
				int_clear();

				if(candidate_taken->data.b_ptbl.eflags & (TAKEN_EFLAG_PF | TAKEN_EFLAG_IOLOCK))
				{
					int_set(0);
					continue;
				}

				/* Get a free ioslot */
				ioslot_id = ioslot_get_free();
				swap_addr = swap_get_addr();

				if(ioslot_id == 0xFFFFFFFF || swap_addr == 0xFFFFFFFF)
				{
					int_set(0);
					continue;
				}

				/* We will do a page out. And it's taken record won't be touched */
				if(candidate_taken->data.b_pg.tbl == 1)
				{
					/* set IOLOCK */
					candidate_taken->data.b_ptbl.eflags |= TAKEN_EFLAG_IOLOCK;
				}
				else
				{
					/* 
					Pages will be paged out immediately, so they generate a PF
					if accesed while swapping. 
					*/
					task = tsk_get(candidate_task_id);

                    if(task == NULL)
                    {
                        // return the ioslot
                        ioslot_return(ioslot_id);
					    int_set(0);
					    continue;
				    }

					/* Set swap info on process table */
					pdir = task->vmm_info.page_directory;
					ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(candidate_laddress)].b));
					
					write = (ptbl->pages[PM_LINEAR_TO_TAB(candidate_laddress)].entry.ia32entry.rw == 1);

					page_out(candidate_task_id, (ADDR)candidate_laddress, 2);

					ptbl->pages[PM_LINEAR_TO_TAB(candidate_laddress)].entry.record.present = 0;
					ptbl->pages[PM_LINEAR_TO_TAB(candidate_laddress)].entry.record.swapped = 1;
					ptbl->pages[PM_LINEAR_TO_TAB(candidate_laddress)].entry.record.addr = swap_addr;
					ptbl->pages[PM_LINEAR_TO_TAB(candidate_laddress)].entry.record.write = write;
					
					/* set the page as free but don't insert it onto the pool */
					task->vmm_info.page_count--;
					task->vmm_info.swap_page_count++;
				}

				/* FIXME: Here we should consider MAPPED FILES */
				ioslot = &ioslots.slots[ioslot_id];
				ioslot->type = IOSLOT_TYPE_SWAP;
				ioslot->dest.swap.swap_addr = swap_addr;
				ioslot->pg_address = PG_ADDRESS(candidate_addr);
				ioslot->finished.callback = vmm_page_swapped_callback;
				ioslot->task_id = candidate_task_id;
				ioslot->owner_type = IOSLOT_OWNER_TYPE_TASK;

				if(candidate_taken->data.b_pg.tbl == 0)
				{
					/* We will page in onto PMAN space here, so we can IO. */
					page_in(PMAN_TASK, (ADDR)(SARTORIS_PROCBASE_LINEAR + candidate_addr), (ADDR)LINEAR2PHYSICAL(candidate_addr), 2, PGATT_WRITE_ENA);
				}

				io_slot_begin_write(vmm.swap_start_index + swap_addr / 0x200, (ADDR)candidate_addr, ioslot_id);
				
				int_set(0);
			}
			else
			{
				int_clear();

				/* Table could have been locked for IO because a page fault raised. */
				if(candidate_taken->data.b_ptbl.tbl == 1 && candidate_taken->data.b_pg.eflags & TAKEN_EFLAG_IOLOCK)
				{
					int_set(0);
					continue;
				}

				/* page out entries not needing IO */
				page_out(candidate_task_id, (ADDR)candidate_laddress, (candidate_taken->data.b_ptbl.tbl == 1)? 1 : 2);

				/* put the page back onto free pages! */
				vmm_put_page((ADDR)LINEAR2PHYSICAL(candidate_addr));
				
				int_set(0);
			}

			iteration++;
		}	// Iterations end

		/* Finished */
		int_clear();
		if(ABS(curr_ag_region - curr_st_region) > vmm.swap_thr_distance)
		{
			curr_st_region++;
			if(curr_st_region == vmm.swap_end_index) 
				curr_st_region = vmm.swap_start_index;
		}
		int_set(0);
	}
}

/*
Calculate page points. The lesser the points, the more likely to be swapped.
*/
UINT32 calculate_points(struct taken_entry *entry, UINT32 pm_dir_index, UINT32 pm_tbl_index, BOOL ioavailable, BOOL *iorequired, UINT16 *task_id, UINT32 *task_laddress)
{
	BOOL d_bit = FALSE;
	BOOL a_bit = FALSE;
	UINT32 age;
	UINT32 swapped = 0;
	BOOL tbl = FALSE;
	UINT32 i = 0;
	struct vmm_pman_assigned_record *assigned;
	struct pm_task *task;
	struct vmm_page_directory *pdir;
	struct vmm_page_table *ptbl;

	/* First things first: Get the directory/table entry for this page on the owner process. */
	if(entry->data.b_pdir.tbl == 0)
	{
		/* It's a common page, get it's table entry */
		UINT32 pman_laddr = pm_dir_index * 0x400000 + pm_tbl_index * 0x1000;	// this is the linear address of the page on pman space
		assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(pman_laddr + (UINT32)SARTORIS_PROCBASE_LINEAR)]->entries[PM_LINEAR_TO_TAB(pman_laddr + (UINT32)SARTORIS_PROCBASE_LINEAR)];

		/* We can now read the entry on pman table, and get the table */
		task = tsk_get(assigned->task_id);
		pdir = task->vmm_info.page_directory;

		if(pdir->tables[assigned->dir_index].ia32entry.present != 1) return 0xFFFFFFFF;

		ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[assigned->dir_index].b));
		
		if(ptbl->pages[entry->data.b_pg.tbl_index].entry.ia32entry.present != 1) return 0xFFFFFFFF;

		d_bit = (ptbl->pages[entry->data.b_pg.tbl_index].entry.ia32entry.dirty == 1);
		a_bit = (ptbl->pages[entry->data.b_pg.tbl_index].entry.ia32entry.accessed == 1);
		age = ptbl->pages[entry->data.b_pg.tbl_index].entry.ia32entry.age;
		tbl = FALSE;
		*task_laddress = assigned->dir_index * 0x400000 + entry->data.b_pg.tbl_index * 0x1000;
		*task_id = assigned->task_id;
	}
	else
	{
		/* It's a page table */
		task = tsk_get(entry->data.b_ptbl.taskid);
		pdir = task->vmm_info.page_directory;
		
		d_bit = 0;
		a_bit = pdir->tables[entry->data.b_ptbl.dir_index].ia32entry.accessed;
		age = pdir->tables[entry->data.b_ptbl.dir_index].ia32entry.age;
		
		ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[entry->data.b_ptbl.dir_index].b));

        struct taken_table *ttable = vmm.taken.tables[PM_LINEAR_TO_DIR(ptbl)];
        UINT32 pmladdr;

		/* Check page is free */
		for(; i < 1024; i++)
		{
			d_bit |= ptbl->pages[i].entry.ia32entry.dirty;
			if(ptbl->pages[i].entry.ia32entry.present == 1) break;
			if(ptbl->pages[i].entry.record.swapped == 1) swapped = 1;

            pmladdr = (UINT32)ptbl + i * 0x1000;
            if(ttable->entries[PM_LINEAR_TO_TAB(pmladdr)].data.b_pg.eflags & TAKEN_EFLAG_IOLOCK) break;
		}

		if(i != 1024) return 0xFFFFFFFF;	// it has present entries
		tbl = TRUE;

		*task_laddress = entry->data.b_ptbl.dir_index * 0x400000;
		*task_id = entry->data.b_ptbl.taskid;
	}

	/* If dirty and no IO is available, return the highest score. */
	if(d_bit && !ioavailable) return 0xFFFFFFFF;

	/* Now calculate points */
	*iorequired = d_bit;

	/* 
	If page is not dirty or it is and io is available we will return: 
	age * [PS_POINTS_DIRTY_MODIFIER] * [PS_POINTS_A_MODIFIER] 
	modifiers will be applied if those bits are set. 
	*/
	return age * PS_POINTS_AGE_MODIFIER + d_bit * PS_POINTS_DIRTY_MODIFIER + a_bit * PS_POINTS_A_MODIFIER + swapped * PS_POINTS_TBL_SWP_MODIFIER + tbl * PS_POINTS_TBL_MODIFIER;
}

/*
Swap writing has finished.
NOTE: This callback function will be called on PMAN context, and not
page stealer thread. DONT cli/sti here.
*/
UINT32 vmm_page_swapped_callback(struct io_slot *ioslot, UINT32 ioret)
{
	UINT32 task_linear;
	UINT16 task_id;
	struct taken_entry *tentry = vmm_taken_get((ADDR)ioslot->pg_address);
	struct pm_task *task = NULL;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;

	if(ioret != IO_RET_ERR)
	{	
		/* if ioslot was invalidated, task was killed */
		if(ioslot->type == IOSLOT_TYPE_INVALIDATED)
		{
			/* Free it's slot on swap */
			swap_free_addr(ioslot->dest.swap.swap_addr);
		}
		else
		{
			task = tsk_get(ioslot->task_id);
				
			if(tentry->data.b_pg.tbl == 1)
			{
				
				/* A page fault might have rised during swap writing on the same table. */
				if(tentry->data.b_ptbl.eflags & TAKEN_EFLAG_PF)
				{
					tentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_PF;

					/* Free it's slot on swap */
					swap_free_addr(ioslot->dest.swap.swap_addr);
					return 0;
				}

				task_linear = tentry->data.b_ptbl.dir_index * 0x400000;
				task_id =  tentry->data.b_ptbl.taskid;

				/* Page out the table :D */
				page_out(task_id, (ADDR)task_linear, 1);

				/* Set table swapped info */
				pdir = task->vmm_info.page_directory;
			
				pdir->tables[tentry->data.b_ptbl.dir_index].record.present = 0;
				pdir->tables[tentry->data.b_ptbl.dir_index].record.swapped = 1;
				pdir->tables[tentry->data.b_ptbl.dir_index].record.addr = ioslot->dest.swap.swap_addr;
				
				task->vmm_info.page_count--;
				task->vmm_info.swap_page_count++;
			}			
		}

		/* put the page back onto free pages! */
		vmm_put_page( (ADDR)PG_ADDRESS(ioslot->pg_address) );
	}
	else
	{
		/* writing failed, remove IOLCK */
		
		/* Free it's slot on swap */
		swap_free_addr(ioslot->dest.swap.swap_addr);

		/* Page could have been locked for IO because a page fault raised. */
		if(tentry->data.b_pg.eflags & TAKEN_EFLAG_PF)
		{
			return 0;
		}

		if(tentry->data.b_pg.tbl == 1)
		{
			tentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_IOLOCK;
		}
		else
		{
			/* put the page back onto free pages for it has been already paged out from the task. */
			vmm_put_page((ADDR)PG_ADDRESS(ioslot->pg_address));
		}		
	}

	return 1;
}
