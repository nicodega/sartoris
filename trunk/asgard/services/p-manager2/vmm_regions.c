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
#include "types.h"
#include "vm.h"
#include "taken.h"
#include "task_thread.h"

/*
Get a Memory Region Descriptor by Type and id. 
REMARKS: If type is 0xFFFF it will be ignored.
*/
struct vmm_region_descriptor *vmm_regiondesc_get(UINT16 id, UINT16 type)
{
	struct vmm_region_descriptor *rdesc = NULL;

	rdesc = vmm.region_descriptors.first;

	while(rdesc != NULL)
	{
		if(rdesc->id == id && (rdesc->type == type || type == 0xFFFF))
			return rdesc;
		rdesc = rdesc->next;
	}

	return NULL;
}

/*
Add a memory region descriptor to global list.
*/
void vmm_regiondesc_add(ADDR prdesc)
{
	struct vmm_region_descriptor *rdesc = (struct vmm_region_descriptor*)prdesc;
	rdesc->next = vmm.region_descriptors.first;
	if(vmm.region_descriptors.first != NULL)
		vmm.region_descriptors.first->prev = rdesc;
	rdesc->prev = NULL;
	vmm.region_descriptors.total++;
	vmm.region_descriptors.first = rdesc;
}

/*
Remove a memory descriptor from global list.
*/
void vmm_regiondesc_remove(ADDR prdesc)
{
	struct vmm_region_descriptor *rdesc = (struct vmm_region_descriptor*)prdesc;

	if(rdesc->prev == NULL)
		vmm.region_descriptors.first = rdesc->next;
	else
		rdesc->prev->next = rdesc->next;

	if(rdesc->next != NULL)
		rdesc->next->prev = rdesc->prev;

	vmm.region_descriptors.total--;
}

/*
Remove a memory descriptor from global list.
*/
void vmm_regiondesc_remove_byid(UINT16 ID)
{
	struct vmm_region_descriptor *rdesc = vmm_regiondesc_get(ID, 0xFFFF);
	vmm_regiondesc_remove(rdesc);
}

/*
This function will return a free memory region descriptor ID.
*/
UINT16 vmm_regiondesc_getid()
{
	UINT16 id = 0;
	struct vmm_region_descriptor *rdesc = NULL;

	if(vmm.region_descriptors.total == 0xFFFF) return 0xFFFF;

	do
	{
		id++;

		rdesc = vmm.region_descriptors.first;

		while(rdesc != NULL)
		{
			if(rdesc->id == id)
				break;
			rdesc = rdesc->next;
		}
	}while(rdesc != NULL);

	return id;
}

/*
Get Page permissions according to the memory region.
*/
UINT32 vmm_region_pageperms(struct vmm_memory_region *mreg)
{
	UINT32 perm = 0;

	if(mreg->flags & VMM_MEM_REGION_FLAG_WRITE)
		perm |= PGATT_WRITE_ENA;

	return perm;
}

/*
Get a memory region from a descriptor.
*/
struct vmm_memory_region *vmm_region_get_bydesc(struct pm_task *task, ADDR desc)
{
	struct vmm_memory_region *mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if(mreg->descriptor == desc)
		{
			break;
		}
		mreg = mreg->next;
	}

	return mreg;
}
/*
Get a memory region for a given task.
*/
struct vmm_memory_region *vmm_region_get(struct pm_task *task, UINT32 tsk_lstart)
{
	struct vmm_memory_region *mreg = task->vmm_inf.regions.first;

	while(mreg != NULL)
	{
		if((UINT32)mreg->tsk_lstart != (UINT32)tsk_lstart)
			break;
		
		mreg = mreg->next;
	}

	return mreg;
}

/*
Add a Memory region on a task.
*/
void vmm_region_add(struct pm_task *task, struct vmm_memory_region *mreg)
{
	mreg->next = task->vmm_inf.regions.first;
	task->vmm_inf.regions.first = mreg;
	task->vmm_inf.regions.total++;
}

/*
Remove a Memory Region from a task.
*/
void vmm_region_remove(struct pm_task *task, struct vmm_memory_region *mreg)
{
	struct vmm_memory_region *cmreg = NULL;

	cmreg = task->vmm_inf.regions.first;

	while(cmreg != NULL && cmreg != mreg)
	{
		if(cmreg->next == mreg) break;
		cmreg = cmreg->next;
	}

	if(task->vmm_inf.regions.first == mreg)
	{
		task->vmm_inf.regions.first = cmreg->next;
	}
	else
	{
		cmreg->next = mreg->next;
	}

	task->vmm_inf.regions.total--;
}



