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

/* Thread parameters */
struct page_aging_t pa_param;

/* This source file contains threads in charge of maintaining 
system available pages on a given threshold. */

/* This will be the aging thread */
void pa_age_pages()
{
	__asm__("sti"::);

	for(;;)
	{
		if(pa_param.ac == 0 || pa_param.finished)
		{
			pa_param.finished = 1;
			continue; // do nothing
		}
		
		pman_print("aging thread alive ub=%x, lb=%x, ac=%i, pass=%i", pa_param.lb, pa_param.ub, pa_param.ac, pa_param.pass);
		
		/* First loop */
		if(!pa_param.finished)
		{
			/* Go through pages on the given region (on the taken tables) */
			unsigned int dindex = PM_LINEAR_TO_DIR(PHYSICAL2LINEAR(pa_param.lb));
			unsigned int tindex = PM_LINEAR_TO_TAB(PHYSICAL2LINEAR(pa_param.lb));

			int nentry = 0, entryc = 0, entry_count = (pa_param.ub - pa_param.lb) / 0x1000;

			// while goes here

			pa_param.pass++;
			if(pa_param.pass == 2)
			{
				pa_param.pass = 0;
			}

			pa_param.finished = 1;
continue;
			while(entryc < entry_count && pa_param.ac > 0)
			{		
				struct taken_entry *entry = &taken->tables[dindex]->entries[nentry + tindex];
				
				__asm__("cli" ::);
			
				/* If it's not a directory page and it does not belong to a service */
				if(IS_TAKEN(entry) && !TAKEN_PDIR(entry) && !((entry->data.b_pg.eflags & TAKEN_EFLAG_SERVICE) || (entry->data.b_pg.eflags & TAKEN_EFLAG_IOLOCK)))
				{
					/* check whether it's a table or a lvl 2 page */
					if(!TAKEN_PTBL(entry) && !(entry->data.b_pg.flags & TAKEN_PG_FLAG_PMAN))
					{
						/* It's a page entry. Get the page table. */
						unsigned int pg_addr = dindex * 0x400000 + tindex * 0x1000;		// this is the linear address of the page on pman space
						unsigned int *pman_tbl = (unsigned int*)PM_TBL(pg_addr);			// this is the process manager table used for this address
						unsigned int pman_tbl_entry = pman_tbl[PM_LINEAR_TO_TAB(pg_addr)];

						/* We can now read the entry on pman table, and get the table */
						unsigned int **prc_pdir = (unsigned int**)PHYSICAL2LINEAR(task_info[PMAN_TBL_GET_TASK(pman_tbl_entry)].page_dir);
						unsigned int *ptbl = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(prc_pdir[PMAN_TBL_GET_DINDEX(pman_tbl_entry)]));
						unsigned int i = entry->data.b_pg.tbl_index;

						/* This is our page! */
						if(pa_param.pass == 0)
						{
							/* Set the A bit (tlb flush will ocurr on thread switch) */
							ptbl[i] &= ~PG_A_BIT;
						}
						else
						{
							/* check A bit and age page */
							if(ptbl[i] & PG_A_BIT)
								ptbl[i] = ((((ptbl[i] >> 9) & 0x7) == 0x7)? ptbl[i] : (ptbl[i] + (0x1 << 9)));
							else
								ptbl[i] = ((((ptbl[i] >> 9) & 0x7) == 0x0)? ptbl[i] : (ptbl[i] - (0x1 << 9)));
						}
					}
					else if(TAKEN_PTBL(entry))
					{
						/* It's a ptable entry */
						unsigned int *pdir = (unsigned int*)PHYSICAL2LINEAR(task_info[entry->data.b_ptbl.taskid].page_dir);
						unsigned int i = entry->data.b_ptbl.dir_index;

						// find page table on dir 
						if(pa_param.pass == 0)
						{
							/* Set the A bit (tlb flush will ocurr on thread switch) */
							pdir[i] &= ~PG_A_BIT;
						}
						else
						{
							/* check A bit and age page */
							if(pdir[i] & PG_A_BIT)
								pdir[i] = ((((pdir[i] >> 9) & 0x7) == 0x7)? pdir[i] : (pdir[i] + (0x1 << 9)));
							else
								pdir[i] = ((((pdir[i] >> 9) & 0x7) == 0x0)? pdir[i] : (pdir[i] - (0x1 << 9)));
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
				pa_param.ac--;
			}

			
		}
	}
}



