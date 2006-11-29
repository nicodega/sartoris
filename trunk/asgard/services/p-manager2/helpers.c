
#include <sartoris/syscall.h>
#include "types.h"
#include "helpers.h"
#include "mem_layout.h"
#include "formats/ia32paging.h"


void map_pages(INT32 task, UINT32 linear_start, UINT32 physical_start, UINT32 count, INT32 flags, INT32 level)
{
	UINT32 i;
	for(i = 0; i < count; i++) 
	{
  		page_in(task, (ADDR) linear_start, (ADDR) physical_start, level, flags);
		linear_start += ((level == 1)? 0x400000 : PAGE_SIZE);
		physical_start += PAGE_SIZE; 
  	}
}
