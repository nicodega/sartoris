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
#include "page_list.h"
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/atac/atac_ioctrl.h>

/* Page alocation IO initialization.
*/
extern int atac_finddev_smo;
extern struct atac_find_dev_param atacparams;
extern page_list free_pages;

void pm_init_page_pool(void *ignore_start, void *ignore_end)  
{
    int i;
    unsigned int offset;

    /* prepare the free page pool */

    /* we need page tables for the process manager, so we can access pool memory */
    
    /* we take the first pages from pool space */
    
    for (i=0; i < POOL_MEGABYTES/4; i++) 
	{  
		page_in(PMAN_TASK, 
				(void*) ((unsigned int)PMAN_POOL_LINEAR + (unsigned int)SARTORIS_PROCBASE_LINEAR + i * 0x400000), 
				(void*) ((unsigned int)PMAN_POOL_PHYS + i * 0x1000), 
				1, 
				PGATT_WRITE_ENA);     
    }

	/* ok, now load whatever is left into the page tables */
  	unsigned int page_count = (POOL_MEGABYTES*0x100000) / 0x1000;

  	for (offset = 0; offset < page_count; offset++) 
	{
		page_in(PMAN_TASK, 
			(void*) ((unsigned int)PMAN_POOL_LINEAR + SARTORIS_PROCBASE_LINEAR + offset * 0x1000), 
			(void*) ((unsigned int)PMAN_POOL_PHYS + offset * 0x1000), 
			2, 
			PGATT_WRITE_ENA);
    }

	/* and now load the pool */
	page_count = ((POOL_MEGABYTES*0x100000) - POOL_TABLES_SIZE) / 0x1000;
    
    init_page_list(&free_pages);

    /* We must be careful here not to put services image pages on the pool */	     
    unsigned int ignore_s = PHYSICAL2LINEAR(ignore_start);
    unsigned int ignore_e = PHYSICAL2LINEAR(ignore_end);

	for (offset = 0; offset < page_count; offset++) 
	{
      	 unsigned int add = FIRST_PAGE(PMAN_POOL_LINEAR) + offset * 0x1000;

		 // Check we are not inserting the init physical pages 
		 if(add < ignore_s || add >= ignore_e)
		 {
			put_page(&free_pages, (void*) (add));
		 }
    }

	/* Pool has been set up, take pages for our taken structure */
	taken = (struct taken_entries*)PHYSICAL2LINEAR(pm_get_page());

	page_count = (POOL_MEGABYTES/4);

	pa_curr_mem = POOL_AVAILABLE;

	/* 
		First 2 tables will be taken by pman and pool page tables, and should never be used.
		They'll remain to be 0.
	*/
	int dentry = PM_LINEAR_TO_DIR(FIRST_PAGE(PMAN_POOL_LINEAR));
	for (i = 0; i <= page_count; i++) 
	{
		unsigned int k = 0, *addr = (unsigned int*)get_page(&free_pages);
	
		while(k < 0x400){ addr[k++] = 0;}

		/* Tell the page alocation system a page has been taken from the pool. */
		pa_page_taken();

		taken->tables[dentry] = (struct taken_table*)addr;
		dentry++;
    }

	struct taken_entry tentry;

	dentry = PM_LINEAR_TO_DIR(FIRST_PAGE(PMAN_POOL_LINEAR));

	/* Set taken structure records as taken by pman */
	for (i = 0; i <= page_count; i++) 
	{
		CREATE_TAKEN_PG_ENTRY(&tentry, TAKEN_PG_FLAG_PMAN, 0, TAKEN_EFLAG_SERVICE);
		pm_assign((void*)LINEAR2PHYSICAL(taken->tables[dentry]), PMAN_TBL_ENTRY_NULL, &tentry );
		dentry++;
    }
}

/* Begin swap initialization */
void pa_init()
{
	/* Issue ATAC ATAC_IOCTRL_FINDLDEV */
	struct atac_ioctl_finddev iocmd;

	iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_FINDLDEV;
	iocmd.msg_id = 0;
	iocmd.ret_port = INITPA_PORT;
	iocmd.find_dev_smo = atac_finddev_smo = share_mem(ATAC_TASK, &atacparams, sizeof(struct atac_find_dev_param), READ_PERM | WRITE_PERM);
	
	atacparams.ptype = 0xe0;
	atacparams.bootable = 0;

	send_msg(ATAC_TASK, 4, &iocmd);
}

/* 
This will return -1 if no swap partition was found, 1 if found, and 0 if atac has not responded yet.
 */
int pa_init_process_msg()
{
	int taskid;
	struct stddev_ioctrl_res res;
	struct thread thr;

	while(get_msg_count(INITPA_PORT) > 0)
	{
		if(atac_finddev_smo != -1)
		{
			get_msg(INITFS_PORT, &res, &taskid);

			/* Message expected from atac */
			if(taskid != ATAC_TASK) continue;

			claim_mem(atac_finddev_smo);
			atac_finddev_smo = -1;
			
			if(res.ret != STDDEV_ERR)
			{
				page_stealing_ldevid = atacparams.ldevid;

				pa_init_data();

				page_stealing_swapstart = atacparams.metadata_end;
				page_stealing_swappages = (atacparams.size - atacparams.metadata_end) / 8;

				//pman_print("Swap device info: start=%x, size=%x pages", page_stealing_swapstart, page_stealing_swappages);

				/* initialize swap bitmap */
				init_swap_bitmap();

				/* Create threads */
				thr.task_num = PMAN_TASK;
				thr.invoke_mode = PRIV_LEVEL_ONLY;
				thr.invoke_level = 3;
				thr.ep = &pa_age_pages;
				thr.stack = (void *)STACK_ADDR(PMAN_AGING_STACK_ADDR);

				if (create_thread(PAGEAGING_THR, &thr) < 0)
				{
					pman_print_and_stop("Could not create page aging thread.");
					STOP;
				}

				thr.task_num = PMAN_TASK;
				thr.invoke_mode = PRIV_LEVEL_ONLY;
				thr.invoke_level = 3;
				thr.ep = &pa_steal_pages;
				thr.stack = (void *)STACK_ADDR(PMAN_STEALING_STACK_ADDR);

				if (create_thread(PAGESTEALING_THR, &thr) < 0)
				{
					pman_print_and_stop("Could not create page stealing thread.");
					STOP;
				}

				return 1;
			}
			else
			{
				page_stealing_ldevid = -1;
				return -1;
			}
		}
	}
	return 0;
}

void init_swap_bitmap()
{
	int i;

	swapbitmap.storage_pages = (page_stealing_swappages / 0x8000) + ((page_stealing_swappages % 0x8000 == 0)? 0 : 1);		// pages on storage
	swapbitmap.storage_free = 0;

	struct taken_entry tentry;
	
	for(i = 0; i < swapbitmap.storage_pages; i++)
	{
		swapbitmap.storage->addr[i] = (unsigned char*)PHYSICAL2LINEAR(pm_get_page());
		CREATE_TAKEN_PG_ENTRY(&tentry, TAKEN_PG_FLAG_PMAN, PM_LINEAR_TO_TAB(swapbitmap.storage->addr[i] + SARTORIS_PROCBASE_LINEAR), 0);
		pm_assign( (void*)LINEAR2PHYSICAL(swapbitmap.storage->addr[i]), PMAN_TBL_ENTRY_NULL, &tentry );
	}
}

/* Page alocation initialization */
extern int direction;

void pa_init_data()
{
	struct taken_entry tentry;
	
	/* Set default values */
	pa_stealing_stop = 0;
	pa_stealing_ticksspan = PA_DEFAULTSPAN;		
	pa_stealing_tickscount = PA_DEFAULT_TICKSCOUNT;	
	pa_stealing_tickscurr = PA_DEFAULT_TICKSCOUNT;	

	pa_stealing_ticks = 0;			
	pa_stealing_direction = direction;			

	/* Timing variables for aging thread */
	pa_aging_stop = 0;
	pa_aging_ticksspan = PA_DEFAULTSPAN;		
	pa_aging_tickscount = PA_DEFAULT_TICKSCOUNT;		
	pa_aging_tickscurr = PA_DEFAULT_TICKSCOUNT;		

	pa_aging_ticks = 0;		
	pa_aging_direction = direction;			

	/* Memory bounds */
	if(POOL_AVAILABLE <= 0x2000000)
	{
		/* Less than 32 MB setup */
		pa_en_mem = 0xA00000;		// 10MB
		pa_des_mem = 0x500000;		// 5MB
		pa_min_mem = 0x200000;		// 2MB

		aging_stealing_maxdistance = POOL_AVAILABLE / 32;
		aging_stealing_mindistance = POOL_AVAILABLE / 64;	// min distance between memory areas for the two processes
		aging_stealing_currdistance = POOL_AVAILABLE / 16;
	}
	else
	{
		pa_en_mem = (POOL_AVAILABLE > (unsigned int)(2^31))? 0x4000000 : 0x2000000;		// 32 / 64 MB
		pa_des_mem = (POOL_AVAILABLE > (unsigned int)(2^31))? 0x2000000 : 0x1000000;	// 32 / 16 MB
		pa_min_mem = (POOL_AVAILABLE > (unsigned int)(2^31))? 0x400000 : 0x200000;		// 4MB / 2MB

		aging_stealing_maxdistance = POOL_AVAILABLE / 32;
		aging_stealing_mindistance = POOL_AVAILABLE / 64;	// min distance between memory areas for the two processes
		aging_stealing_currdistance = POOL_AVAILABLE / 16;
	}

	page_stealing_iocount = 0;

	/* Thread parameters */
	pa_param.pass = 0;
	pa_param.lb = FIRST_PAGE(PMAN_POOL_PHYS);
	pa_param.ub = PMAN_POOL_PHYS + PA_REGIONSIZE - POOL_TABLES_SIZE;
	pa_param.ac = PA_REGIONSIZE / 0x1000;
	pa_param.finished = 0;

	ps_param.lb = PMAN_POOL_PHYS + aging_stealing_currdistance;
	ps_param.ub = ps_param.lb + PA_REGIONSIZE;
	ps_param.sc = 0;	// will be adjusted at the pa scheduler
	ps_param.finished = 0;
	ps_param.last_freed = ps_param.lb;
	ps_param.fc = 0;
	ps_param.iterations = 0;

	min_hits = 0;

	/* Initialize IO pending structure */
	iopending = (struct page_stealing_iopending*)PHYSICAL2LINEAR(pm_get_page());

	CREATE_TAKEN_PG_ENTRY(&tentry, TAKEN_PG_FLAG_PMAN, PM_LINEAR_TO_TAB(iopending), 0);
	pm_assign( (void*)LINEAR2PHYSICAL(iopending), PMAN_TBL_ENTRY_NULL, &tentry );

	iopending->free_first = 0;
	int i = 0;
	for(i = 0; i < 1022; i++)
	{
		iopending->addr[i] = (i+1) | PA_IOPENDING_FREEMASK;
	}
	iopending->addr[1022] = 0xFFFFFFFF;

	/* Initialize swap bitmap structure (but not storage) */
	swapbitmap.storage = (struct page_stealing_swapbitmap_storage*)PHYSICAL2LINEAR(pm_get_page());

	CREATE_TAKEN_PG_ENTRY(&tentry, TAKEN_PG_FLAG_PMAN, PM_LINEAR_TO_TAB(swapbitmap.storage->addr[i]), 0);
	pm_assign( (void*)LINEAR2PHYSICAL(swapbitmap.storage), PMAN_TBL_ENTRY_NULL, &tentry );

	for(i = 0; i < 1024; i++)
	{
		swapbitmap.storage->addr[i] = NULL;
	}
}


