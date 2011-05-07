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
Get a memory region from a descriptor for a given task.
*/
struct vmm_memory_region *vmm_region_get_bydesc(struct pm_task *task, struct vmm_descriptor *descriptor)
{
	struct vmm_memory_region *mreg = descriptor->regions;

	while(mreg != NULL)
	{
		if(mreg->owner_task == task->id)
		{
			break;
		}
		mreg = mreg->next;
	}

	return mreg;
}

