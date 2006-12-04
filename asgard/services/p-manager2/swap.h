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


#ifndef SWAPH
#define SWAPH


/* A structure used to keep track of slots used on swap file */
struct vmm_swap_bitmap
{
	unsigned int storage_pages;		// pages on storage
	unsigned int storage_free;		
	struct vmm_swap_bitmap_storage *storage;
} PACKED_ATT;

struct vmm_swap_bitmap_storage
{
	unsigned char *addr[1024];		// the bitmap will be composed of up to 1024 pages
} PACKED_ATT;

void swap_free_addr(UINT32 swap_addr);
void swap_use_addr(UINT32 swap_addr, UINT32 perms);
UINT32 swap_get_perms(struct pm_task *task, ADDR proc_laddress);
UINT32 swap_get_addr();
void init_swap_bitmap();

/* Point modifiers on the stealing thread for page candidate selection. */
#define PS_POINTS_TBL_MODIFIER		2	// page is a page table
#define PS_POINTS_TBL_SWP_MODIFIER	2	// page is a page table with records swapped (requires IO)
#define PS_POINTS_DIRTY_MODIFIER	4	// page is dirty (requires IO)
#define PS_POINTS_A_MODIFIER		2	// A bit is turned on.
#define PS_POINTS_AGE_MODIFIER		2	// This value will be multiplied by the page age.

#endif
