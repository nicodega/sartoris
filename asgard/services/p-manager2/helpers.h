
#include "types.h"

#ifndef HELPERSH
#define HELPERSH

#define MIN(a,b) ((a < b)? a : b)
#define MAX(a,b) ((a < b)? b : a)

void map_pages(INT32 task, UINT32 linear_start, UINT32 physical_start, UINT32 count, INT32 flags, INT32 level);

#define STOP for(;;);

#endif

