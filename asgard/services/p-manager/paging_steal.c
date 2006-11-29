/*
*	Process and Memory Manager Service
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

#include <os/layout.h>
#include "pmanager_internals.h"
#include <services/stds/stdblockdev.h>
#include "page_list.h"

unsigned int page_stealing_iocount;		// current io operations
unsigned int page_stealing_ldevid;		// logic device for swap file
unsigned int page_stealing_swappages;	// pages available on swapfile
unsigned int page_stealing_swapstart;	// first usable sector on swap file

/* Thread parameters */
struct page_stealing_t ps_param;

struct page_stealing_iopending *iopending = NULL;
struct page_stealing_swapbitmap swapbitmap;

/* This source file contains threads in charge of maintaining 
system available pages on a given threshold. */
void steal_finish_io();
unsigned int calculate_points(unsigned int tbl_entry, unsigned int *tbl_ptr, int ioavailable);

/* This will be the stealing thread */
void pa_steal_pages()
{
	__asm__("sti"::);

	unsigned int candidate_paddr;						// candidate page physical address
	unsigned int candidate_dindex, candidate_tindex;	// candidate taken structure indexes
	unsigned int candidate_pman_entry_addr;				// candidate process manager page table entry address
	unsigned int candidate_points = (unsigned int)-1;	// candidate points
	unsigned int candidate_task_linear;
	unsigned int candidate_task;
	int candidate_io = 0, candidate_tbl;
	int task_id;

	/* 
		NOTE: remember to cli/sti while inserting a page 
		on the free pool!! so pman won't interrupt 
		at the middle of the operation 
	*/

	/*
		NOTE2: Page faults are non maskable interrupts, but this implies no 
		problem for us if we cli when modifying the free_pages list
		for we won't page fault, and we will be the only one running.
	*/

	for(;;)
	{
		/* Finish pending io operations */
		steal_finish_io();

		if(ps_param.sc == 0 || pa_param.finished)
		{
		pman_print("steal finished");

			pa_param.finished = 1;
			continue; // do nothing
		}

		/* Loop until we get as many pages as sc */
		if(!pa_param.finished)
		{
			candidate_points = (unsigned int)-1;
			candidate_io = 0;

			/* Go through pages on the given region (on the taken tables) and select a candidate for removal */
			unsigned int dindex = PM_LINEAR_TO_DIR(PHYSICAL2LINEAR(ps_param.lb));
			unsigned int tindex = PM_LINEAR_TO_TAB(PHYSICAL2LINEAR(ps_param.lb));

			int nentry = 0, entryc = 0, entry_count = (ps_param.ub - ps_param.lb) / 0x1000;
			unsigned int pages_count, ptbl_count;	// candidate pages and tables.

			while(entryc < entry_count && ps_param.sc > 0)
			{
				
				struct taken_entry *entry = &taken->tables[dindex]->entries[nentry + tindex];
				
				__asm__("cli" ::);
			
				if(IS_TAKEN(entry) && !TAKEN_PDIR(entry) && !((entry->data.b_pg.eflags & TAKEN_EFLAG_SERVICE) || (entry->data.b_pg.eflags & TAKEN_EFLAG_IOLOCK)))
				{
					if(!TAKEN_PTBL(entry) || (TAKEN_PTBL(entry) && ps_param.iterations > PA_TBL_ITERATIONS && ps_param.fc > (ps_param.sc / 2)))
					{
						/* ignore pman pages */
						if(!TAKEN_PTBL(entry) && (entry->data.b_pg.flags & TAKEN_PG_FLAG_PMAN))
						{
							__asm__("sti" ::);
							continue;
						}

						unsigned int tbl_entry;
						unsigned int *tbl_ptr = 0;
						unsigned int pg_addr, *pman_tbl, pman_tbl_entry;
						unsigned int task_linear;

						/* calculate points and get it's dir/table entry */
						if(!TAKEN_PTBL(entry) && !(entry->data.b_pg.flags & TAKEN_PG_FLAG_PMAN))
						{
							/* It's a page entry. Get the page table. */
							pg_addr = dindex * 0x400000 + tindex * 0x1000;		// this is the linear address of the page on pman space
							pman_tbl = (unsigned int*)PM_TBL(pg_addr);			// this is the process manager table used for this address
							pman_tbl_entry = pman_tbl[PM_LINEAR_TO_TAB(pg_addr)];

							/* We can now read the entry on pman table, and get the table */
							unsigned int **prc_pdir = (unsigned int**)PHYSICAL2LINEAR(task_info[PMAN_TBL_GET_TASK(pman_tbl_entry)].page_dir);
							unsigned int *ptbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(prc_pdir[PMAN_TBL_GET_DINDEX(pman_tbl_entry)]));
							
							tbl_entry = ptbl[(int)entry->data.b_pg.tbl_index];
							task_linear = PMAN_TBL_GET_DINDEX(pman_tbl_entry) * 0x400000 + entry->data.b_pg.tbl_index * 0x1000;
							task_id = (unsigned int)PMAN_TBL_GET_TASK(pman_tbl_entry);
						}
						else if(TAKEN_PTBL(entry))
						{
							/* It's a ptable entry */
							unsigned int *pdir = (unsigned int*)PHYSICAL2LINEAR(task_info[entry->data.b_ptbl.taskid].page_dir);

							tbl_entry = pdir[(int)entry->data.b_ptbl.dir_index];	
							tbl_ptr = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(tbl_entry));
							task_linear = entry->data.b_ptbl.dir_index * 0x400000;
							task_id = (unsigned int)entry->data.b_ptbl.taskid;
						}

						unsigned int points = calculate_points(tbl_entry, tbl_ptr, (page_stealing_iocount < PA_IOPENDING_SLOTS && swap_get_addr() != 0xFFFFFFFF) );

						if(tbl_entry & PG_D_BIT) candidate_io = 1;

						/* Compare this page to our current candidate */
						if(points < candidate_points)
						{
							candidate_points = points;
							candidate_paddr = PG_ADDRESS(tbl_entry);
							candidate_dindex = PM_LINEAR_TO_DIR(PHYSICAL2LINEAR(candidate_paddr));
							candidate_tindex = PM_LINEAR_TO_TAB(PHYSICAL2LINEAR(candidate_paddr));
							candidate_pman_entry_addr = PM_TBL(PHYSICAL2LINEAR(candidate_paddr));
							candidate_tbl = TAKEN_PTBL(entry);
							candidate_task_linear = task_linear;
							candidate_task = task_id;
						}
						
						if(TAKEN_PTBL(entry))
						{
							ptbl_count++;
						}
						else
						{
							pages_count++;
						}
					}
				}

				__asm__("sti" ::);

				nentry++;
				if(nentry + tindex > 1023)
				{
					nentry = 0;
					dindex++;
					tindex = 0;
				}
				entryc++;
			}

			/* Check if we had a candidate */
			if(candidate_points == 0xFFFFFFFF)
			{
				pa_param.finished = 1;
				continue;
			}

			struct taken_entry *tentry;

			tentry = get_taken((void*)PG_ADDRESS(candidate_paddr));
			
			/* We've got our candidate. */
			if(candidate_io)
			{
				/* begin io operations for pages which need to be preserved on the swap file,
				setting page physical address on page_stealing_iopending */
				struct stdblockdev_writem_cmd writecmd;

				writecmd.command = BLOCK_STDDEV_WRITEM;
				writecmd.count = 8;
				writecmd.dev = page_stealing_ldevid;
				writecmd.ret_port = SWAP_WRITE_PORT;

				/* get a free swap slot */
				unsigned int swap_addr = swap_get_addr();

				/* We will do a page out. And it's taken record won't be touched */
				__asm__("cli" ::);

				/* get a free slot for IO */
				int ioslot = iopending->free_first;

				/* fix free first and count */
				page_stealing_iocount++;
				iopending->free_first = iopending->addr[iopending->free_first] & ~PA_IOPENDING_FREEMASK;
	
				if(candidate_tbl)
				{
					tentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_PF | TAKEN_EFLAG_IOLOCK;
					pm_assign((void*)candidate_paddr, PMAN_TBL_ENTRY_NULL, tentry);
				}
				else
				{
					/* Pages will be paged out immediately, so they generate a PF
					if accesed while swapping. */
					page_out(candidate_task, candidate_task_linear, 2);

					/* Set swap info on table */
					unsigned int *tsk_tbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(((unsigned int**)PHYSICAL2LINEAR(task_info[candidate_task].page_dir))[PM_LINEAR_TO_DIR(candidate_task_linear)]));
					tsk_tbl[PM_LINEAR_TO_TAB(candidate_task_linear)] = PG_ADDRESS(swap_addr) | PA_TASKTBL_SWAPPED_FLAG;

					/* set the page as free but don't insert it onto the pool */
					struct taken_entry fentry;
					fentry.data.b = 0;
					set_taken(&fentry, (void*)candidate_paddr);

					task_info[candidate_task].page_count--;
					task_info[candidate_task].swap_page_count++;
				}

				iopending->addr[ioslot] = PG_ADDRESS(candidate_paddr);
				iopending->swap_addr[ioslot] = swap_addr;

				if(!candidate_tbl)
				{
					/* We will page in onto PMAN space here, so we can IO. */
					pm_page_in(PMAN_TASK, (void*)(SARTORIS_PROCBASE_LINEAR + PHYSICAL2LINEAR(candidate_paddr)), (void*)candidate_paddr, 2, PGATT_WRITE_ENA);
				}

				writecmd.pos = page_stealing_swapstart + swap_addr / 0x200;
				writecmd.buffer_smo = share_mem(ATAC_TASK, PHYSICAL2LINEAR(candidate_paddr), 0x1000, READ_PERM|WRITE_PERM);
				writecmd.msg_id = ioslot;

				/* Send IO! */
				send_msg(ATAC_TASK, 4, &writecmd);

				__asm__("sti" ::);
			}
			else
			{
				__asm__("cli" ::);

				/* Table could have been locked for IO because a page fault raised. */
				if(candidate_tbl && tentry->data.b_pg.eflags & TAKEN_EFLAG_PF)
				{
					__asm__("sti" ::);
					continue;
				}

				/* page out entries not needing io */
				page_out(task_id, candidate_task_linear, (candidate_tbl)? 1 : 2);

				/* Set the table entry to 0 (not swapped, nor present) */
				if(candidate_tbl)
				{
					((unsigned int*)PHYSICAL2LINEAR(task_info[candidate_task].page_dir))[PM_LINEAR_TO_DIR(candidate_task_linear)] = 0;
				}
				else
				{
					unsigned int *tsk_tbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(((unsigned int**)PHYSICAL2LINEAR(task_info[candidate_task].page_dir))[PM_LINEAR_TO_DIR(candidate_task_linear)]));
					tsk_tbl[PM_LINEAR_TO_TAB(candidate_task_linear)] = 0;
				}

				/* put the page back onto free pages! */
				pm_put_page((void*)LINEAR2PHYSICAL(dindex * 0x400000 + tindex * 0x1000));
				
				__asm__("sti" ::);
			}

			ps_param.sc--;
			ps_param.fc--;
			ps_param.last_freed = candidate_paddr;
			ps_param.iterations++;

			if(ps_param.iterations == PA_MAX_ITERATIONS)
			{
				pa_param.finished = 1;
			}
		}
	}
}

void steal_finish_io()
{
	struct stdblockdev_res res;
	int id;
	unsigned int task_linear;
	int task_id;

	/* Finish pending io operations */
	while(get_msg_count(SWAP_WRITE_PORT) > 0)
	{
		get_msg(SWAP_WRITE_PORT, &res, &id);

		if(id == ATAC_TASK && res.msg_id <= PA_IOPENDING_SLOTS-1)
		{
			/* Check the io msg id, and get the page_stealing_iopending physical address */
			int pg_paddr = iopending->addr[res.msg_id];

			if(iopending->addr[res.msg_id] & PA_IOPENDING_FREEMASK) continue;

			/* free the io slot */
			page_stealing_iocount--;
			iopending->addr[res.msg_id] = iopending->free_first | PA_IOPENDING_FREEMASK;
			iopending->free_first = res.msg_id;

			struct taken_entry *tentry = get_taken((void*)PG_ADDRESS(pg_paddr));
			
			if(res.ret != STDBLOCKDEV_ERR)
			{	
				__asm__("cli" ::);

				if(TAKEN_PTBL(tentry))
				{
					/* A page fault might have rised during swap writing on the same table. */
					if(tentry->data.b_ptbl.eflags & TAKEN_EFLAG_PF)
					{
						/* Free it's slot on swap */
						swap_free_addr(iopending->swap_addr[res.msg_id]);

						__asm__("sti" ::);
						continue;
					}

					task_linear = tentry->data.b_ptbl.dir_index * 0x400000;
					task_id =  tentry->data.b_ptbl.taskid;

					/* Page out the table :D */
					page_out(task_id, task_linear, 1);

					/* Set table swapped info */
					((unsigned int*)PHYSICAL2LINEAR(task_info[task_id].page_dir))[PM_LINEAR_TO_DIR(task_linear)] = PG_ADDRESS(iopending->swap_addr[res.msg_id]) | PA_TASKTBL_SWAPPED_FLAG;

					task_info[task_id].page_count--;
					task_info[task_id].swap_page_count++;
				}
				
				/* put the page back onto free pages! */
				pm_put_page( (void*)PG_ADDRESS(pg_paddr) );
				
				__asm__("sti" ::);
			}
			else
			{
				/* writing failed, remove IOLCK */
				__asm__("cli" ::);

				/* Free it's slot on swap */
				swap_free_addr(iopending->swap_addr[res.msg_id]);

				/* Page could have been locked for IO because a page fault raised. */
				if(tentry->data.b_pg.eflags & TAKEN_EFLAG_PF)
				{
					__asm__("sti" ::);
					continue;
				}

				if(TAKEN_PTBL(tentry))
				{
					tentry->data.b_ptbl.eflags &= ~TAKEN_EFLAG_IOLOCK;
				}
				else
				{
					/* put the page back onto free pages for it has been already paged out from the task. */
					pm_put_page((void*)PG_ADDRESS(pg_paddr) );
				}
				
				__asm__("sti" ::);
			}
		}
	}
}

/* This function will calculate a page points.
 Greater points means page is a worst candidate for removal. */
unsigned int calculate_points(unsigned int tbl_entry, unsigned int *tbl_ptr, int ioavailable)
{
	if((unsigned int)tbl_ptr != 0)
	{
		/* If it's not a table we will check if page is dirty and io is available */
		if((tbl_entry & PG_D_BIT) && !ioavailable) return (unsigned int)-1;

		/* If page is not dirty or it is and io is available we will return: 
		age * [PA_POINTS_DIRTY_MODIFIER] * [PA_POINTS_A_MODIFIER] 
		modifiers will be applied if those bits are set. */
		return ((tbl_entry >> 9) & 0x7) * PA_POINTS_AGE_MODIFIER + ((tbl_entry & PG_D_BIT)? PA_POINTS_DIRTY_MODIFIER : 1 ) + ((tbl_entry & PG_A_BIT)? PA_POINTS_A_MODIFIER : 1 );
	}
	else
	{
		/* Ok this is a page table. We must see if the table is empty first. */
		int i = 0, swapped = 0;

		for(; i < 1024; i++)
		{
			if(PG_PRESENT(tbl_ptr[i])) break;
			if(PA_TASKTBL_SWAPPED(tbl_ptr[i]))
			{
				swapped = 1;
			}
		}

		if(i != 1024) return (unsigned int)-1;	// it has present entries
		if((tbl_entry & PG_D_BIT) && !ioavailable) return (unsigned int)-1;

		return ((tbl_entry >> 9) & 0x7) * PA_POINTS_AGE_MODIFIER + ((tbl_entry & PG_D_BIT)? PA_POINTS_DIRTY_MODIFIER : 1 ) + ((tbl_entry & PG_A_BIT)? PA_POINTS_A_MODIFIER : 1 ) + ((swapped)? PA_POINTS_TBL_SWP_MODIFIER : 1) + PA_POINTS_TBL_MODIFIER;
	}
}


