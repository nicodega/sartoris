
#include "lib/containers.h"

#ifndef _SALLOCH_
#define _SALLOCH_

#define SALLOC_THR   CONT_ALLOC_THR
#define SALLOC_TSK   CONT_ALLOC_TSK
#define SALLOC_SMO   CONT_ALLOC_SMO
#define SALLOC_PRT   CONT_ALLOC_PRT
#define SALLOC_MSG   CONT_ALLOC_MSG

#define SALLOC_TYPES 5

#define SALLOC2CONT_TYPE(type) (type)

/*
Allocate an object. If type is: SALLOC_THR, SALLOC_TSK or SALLOC_SMO
an id must be provided. If not, it will be ignored.
*/
void *salloc(int id, int type);
/*
Free a given structure.
Ptr must be NOT NULL *only* if type is not SALLOC_THR,  SALLOC_TSK or SALLOC_SMO
*/
void sfree(void *ptr, int id, int type);

void init_salloc();

#endif
