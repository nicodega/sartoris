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

#define ABS(a) ((a > 0)? a : -1 * a)

extern UINT32 curr_st_region;
extern UINT32 curr_ag_region;

/*
This will be the aging thread.
*/
void vmm_page_aging()
{
	struct taken_table *ttable;
	struct vmm_page_table *ptbl;
	UINT32 pman_laddr, tbl_index, b, tindex, pass, dir_index;
	struct vmm_pman_assigned_record *assigned;
	struct pm_task *task;
	struct vmm_page_directory *pdir;

	/* Go through taken entries.*/
	int_set(0);

	for(;;)
	{
		tindex = 0;
		pass = 0;

		/* Get our current region taken table. */
		ttable = vmm.taken.tables[curr_ag_region];

		/* Cicle through taken entries on this region. */
		for(tindex = 0; tindex < 1024; tindex++)
		{
			struct taken_entry *entry = &ttable->entries[tindex];

			int_clear();
			
			/* If it's not a directory page and it does not belong to a service */
			if(entry->data.b_pg.taken == 1 && entry->data.b_pg.dir != 1 && !((entry->data.b_pg.eflags & TAKEN_EFLAG_SERVICE) || (entry->data.b_pg.eflags & TAKEN_EFLAG_IOLOCK)))
			{
				/* Check whether it's a table or a lvl 2 page */
				if(entry->data.b_pdir.tbl == 0 && !(entry->data.b_pg.flags & TAKEN_PG_FLAG_PMAN))
				{
					/* It's a page entry. Get the page table. */
					pman_laddr = curr_ag_region * 0x400000 + tindex * 0x1000;	// this is the linear address of the page on pman space
					assigned = &vmm.assigned_dir.table[PM_LINEAR_TO_DIR(pman_laddr + (UINT32)SARTORIS_PROCBASE_LINEAR)]->entries[PM_LINEAR_TO_TAB(pman_laddr + (UINT32)SARTORIS_PROCBASE_LINEAR)];

					/* We can now read the entry on pman table, and get the table */
					task = tsk_get(assigned->task_id);
					pdir = task->vmm_inf.page_directory;

					if(pdir->tables[assigned->dir_index].ia32entry.present != 1)
					{
						int_set(0);
						continue;              // page table is not present, ignore record
					}
					
					ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[assigned->dir_index].b));
					
					if(ptbl->pages[entry->data.b_pg.tbl_index].entry.ia32entry.present != 1)
					{
						int_set(0);
						continue;              // page is not present
					}

					tbl_index = entry->data.b_pg.tbl_index;

					/* This is our page! */
					if(pass == 0)
					{
						/* Set the A bit (tlb flush will ocurr on thread switch) */
						ptbl->pages[tbl_index].entry.ia32entry.accessed = 1;
					}
					else
					{
						b = ptbl->pages[tbl_index].entry.phy_page_addr;

						/* check A bit and age page */
						if(ptbl->pages[tbl_index].entry.ia32entry.accessed == 1)
							ptbl->pages[tbl_index].entry.phy_page_addr = ((((ptbl->pages[tbl_index].entry.phy_page_addr >> 9) & 0x7) == 0x7)? b : (b + (0x1 << 9)));
						else
							ptbl->pages[tbl_index].entry.phy_page_addr = ((((ptbl->pages[tbl_index].entry.phy_page_addr >> 9) & 0x7) == 0x0)? b : (b - (0x1 << 9)));
					}
				}
				else if(entry->data.b_pdir.tbl == 1)
				{
					/* It's a ptable entry */
					task = tsk_get(entry->data.b_ptbl.taskid);
					pdir = task->vmm_inf.page_directory;
					
					dir_index = entry->data.b_ptbl.dir_index;

					/* This is our page! */
					if(pass == 0)
					{
						/* Set the A bit (tlb flush will ocurr on thread switch) */
						pdir->tables[dir_index].ia32entry.accessed = 1;
					}
					else
					{
						b = pdir->tables[dir_index].b;

						/* check A bit and age page */
						if(pdir->tables[dir_index].ia32entry.accessed == 1)
							pdir->tables[dir_index].b = ((((pdir->tables[dir_index].b >> 9) & 0x7) == 0x7)? b : (b + (0x1 << 9)));
						else
							pdir->tables[dir_index].b = ((((pdir->tables[dir_index].b >> 9) & 0x7) == 0x0)? b : (b - (0x1 << 9)));
					}				
				}
			}

			int_set(0);
		}

		if(pass == 0)
		{
			pass++;
		}
		else
		{
			pass = 0;

			/* Finished */
			int_clear();
			if(ABS(curr_ag_region - curr_st_region) > vmm.swap_thr_distance)
			{
				curr_ag_region++;
				if(curr_ag_region == vmm.swap_end_index) curr_ag_region = vmm.swap_start_index;
			}
			int_set(0);
		}
	}
}

