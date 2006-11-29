
#include "sartoris/kernel.h"
#include "sartoris/syscall.h"
#include "types.h"
#include "task_thread.h"
#include "vm.h"
#include "taken.h"
#include "exception.h"

/*
Attempt to grow a task max_addr
*/
UINT32 vmm_grow_task(struct pm_task *task, UINT32 size)
{
	/* 
	Max address is fixed at 1 GB for now. 

	When implemented, I should do something like this:
	
	|	0x00000000
	|
	|		Sartoris Virtual space
	|
	|	0x00080000
	|		Exec + data + bss?
	|	
	|	0xXXXXXXXX <- End of Exec + data + bss (Maybe bounded by 1GB)
	|
	|		Brk Space (somewhere here will be task->vmm_inf.max_addr)
	|
	|	0x80000000 (2GB)
	|
	|		Shared libs Space
	|
	|	0xE0000000 (3.5 GB)
	|
	|		Stack Available space (512MB)
	|
	|	0xFFFFFFFF

	Pointer to 0xXXXXXXXX will be left on stack. This way, malloc can be initialized 
	with 0xXXXXXXXX - Delta, where Delta ys the initial ammount of memory granted.

	Delta = 3200000 (50 MB of initial brk)
	Delta = 1900000 (25 MB of initial brk)
	*/
	return 0;
}

UINT32 vmm_shink_task(struct pm_task *task, UINT32 size)
{
	/* 
		Shrink without compromising regions.
		Check task->vmm_inf.max_addr - size is greater or equal to 0xXXXXXXXX

		NOTE: When shinking, unmap (won't send to swap) pages on the region: task->vmm_inf.max_addr - size
		Set task->vmm_inf.max_addr to task->vmm_inf.max_addr - size.
		If swapped, release from swap.
	*/
	return 0;
}

