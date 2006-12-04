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


#include "types.h"
#include "vm.h"
#include "swap.h"
#include "task_thread.h"
#include <sartoris/kernel.h>

/* Swap Bitmap structure. */

struct vmm_swap_bitmap swapbitmap;

/* Set swap address as free on the bitmap */
void swap_free_addr(UINT32 swap_addr)
{
	UINT32 slinaddr, pindex, index, shift;

	slinaddr = swap_addr / 8;

	pindex = slinaddr / 0x1000;
	index = slinaddr % 0x1000;
	shift = 7 - (swap_addr % 8);

	swapbitmap.storage->addr[pindex][index] = swapbitmap.storage->addr[pindex][index] & ~(0x1 << shift);

	if(swapbitmap.storage_free > slinaddr)
	{
		swapbitmap.storage_free = slinaddr;
	}

	vmm.swap_available++;
}

/* set swap address as taken on the bitmap */
void swap_use_addr(UINT32 swap_addr, UINT32 perms)
{
	UINT32 slinaddr, pindex, index, shift;

	vmm.swap_available--;

	slinaddr = swap_addr / 8;

	pindex = slinaddr / 0x1000;
	index = (slinaddr % 0x1000);
	shift = 7 - (swap_addr % 8);

	swapbitmap.storage->addr[pindex][index] = swapbitmap.storage->addr[pindex][index] | (0x1 << shift);
	
	if(swapbitmap.storage_free == slinaddr)
	{
		while(swapbitmap.storage->addr[pindex][index] == 0xFF)
		{
			index++;
			if(index == 0x1000)
			{
				index = 0;
				pindex++;
				if(pindex == swapbitmap.storage_pages)
				{
					swapbitmap.storage_free = 0xFFFFFFFF;
					return;
				}
			}
		}
		swapbitmap.storage_free = pindex * 0x1000 + index;
	}
}

/* Get a free address on swap file */
UINT32 swap_get_addr()
{
	UINT32 pindex, index, i;

	if(swapbitmap.storage_free == 0xFFFFFFFF) return 0xFFFFFFFF;

	pindex = swapbitmap.storage_free / 0x1000;
	index = swapbitmap.storage_free % 0x1000;
	i = 7;

	while(i >= 0)
	{
		if( !(swapbitmap.storage->addr[pindex][index] & (0x1 << i)) ) break;
		i++;
	}

	return (pindex * 0x1000 + index) * 0x1000;
}

void init_swap_bitmap()
{
	UINT32 i;

	swapbitmap.storage_pages = (vmm.swap_size / 0x8000) + ((vmm.swap_size % 0x8000 == 0)? 0 : 1);		// pages on storage
	swapbitmap.storage_free = 0;
	
	for(i = 0; i < swapbitmap.storage_pages; i++)
	{
		swapbitmap.storage->addr[i] = (BYTE*)vmm_pm_get_page(FALSE);
		i++;
	}
}

/*
Get page permissions from not present record.
*/
UINT32 swap_get_perms(struct pm_task *task, ADDR proc_laddress)
{
	UINT32 perms = 0;
	struct vmm_page_directory *pdir = NULL;
	struct vmm_page_table *ptbl = NULL;

	pdir = task->vmm_inf.page_directory;
	ptbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(pdir->tables[PM_LINEAR_TO_DIR(proc_laddress)].b));

	if(ptbl->pages[PM_LINEAR_TO_TAB(proc_laddress)].entry.record.write == 1)
		perms = PGATT_WRITE_ENA;
	
	return perms;
}


