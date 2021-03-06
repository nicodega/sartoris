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


#ifndef IA32PAGINGH
#define IA32PAGINGH

#include "../types.h"

/* IA32 page table entry */
struct vmm_page_tbl_entry
{
	UINT32 present:1;		// If 0 this should be considered as a vmm_not_present_record
	UINT32 rw:1;
	UINT32 us:1;
	UINT32 write_through:1;
	UINT32 cache_disable:1;
	UINT32 accessed:1;
	UINT32 dirty:1;
	UINT32 O_bit:1;
	UINT32 g_bit:1;
	UINT32 age:3;
	UINT32 addr:20;	
} PACKED_ATT;

/* IA32 Page directory entry */
struct vmm_dir_entry
{
	UINT32 present:1;		// If 0 this should be considered as a vmm_not_present_record
	UINT32 rw:1;
	UINT32 us:1;
	UINT32 write_through:1;
	UINT32 cache_disable:1;
	UINT32 accessed:1;
	UINT32 reserved:1;
	UINT32 O_bit:1;
	UINT32 g_bit:1;
	UINT32 age:3;
	UINT32 addr:20;	
} PACKED_ATT;

/* Linear address to Directory Address */
#define PM_LINEAR_TO_DIR(linear)	((UINT32)linear >> 22)
/* Linear address to Table Address */
#define PM_LINEAR_TO_TAB(linear)	(((UINT32)linear & 0x003FFFFF) >> 12)
/* Page address from physial address */
#define PG_ADDRESS(physical)		((UINT32)physical & 0xFFFFF000)
/* Page table address from physial address */
#define TBL_ADDRESS(physical)		((UINT32)physical & 0xFFC00000)
/* Check if page is present on a Table/Dir Entry */
#define PG_PRESENT(entry)			((UINT32)entry & 0x00000001)

/* Intel A/Dirty flags. */
#define PG_A_BIT 0x20
#define PG_D_BIT 0x40

#define PAGE_SIZE                   0x1000

#define PF_ERRORCODE_PROTECTION     0x1
#define PF_ERRORCODE_WR             0x2
#define PF_ERRORCODE_USER           0x4
#define PF_ERRORCODE_RSVD           0x8
#define PF_ERRORCODE_INSTRUCTION    0x10

#endif

