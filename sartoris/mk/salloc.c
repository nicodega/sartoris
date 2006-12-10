

#include "lib/containers.h"
#include "lib/salloc.h"
#include "lib/indexing.h"

int ammount[SALLOC_TYPES];
int max_ammount[SALLOC_TYPES];

void init_salloc()
{
	int i = 0;
	for(;i<SALLOC_TYPES;i++)
		ammount[i] == 0;

	max_ammount[SALLOC_THR] = MAX_THR;
	max_ammount[SALLOC_TSK] = MAX_TSK;
	max_ammount[SALLOC_SMO] = MAX_SMO;
	max_ammount[SALLOC_MSG] = MAX_MSG;
	max_ammount[SALLOC_PRT] = MAX_TSK_OPEN_PORTS * MAX_TSK;
}

/*
Allocate a given structure by id
*/
void *salloc(int id, int type)
{	
	if(ammount[type] == max_ammount[type]) return NULL;

	if(type == SALLOC_TSK || type == SALLOC_THR || type == SALLOC_SMO)
	{
		if((type == SALLOC_TSK && TST_PTR(id,tsk)) || (type == SALLOC_THR && TST_PTR(id,thr)) || (type == SALLOC_SMO && TST_PTR(id,smo)))
			return NULL;	// object already exists

		if(!index_alloc(id, ALLOC2IDX(type)))
		{
			return NULL;
		}

		void * ptr = csalloc(SALLOC2CONT_TYPE(type)); 

		if(ptr == NULL)
		{
			index_free(id, ALLOC2IDX(type));
			return NULL;
		}

		switch(type)
		{
			case SALLOC_TSK:
				GET_PTR(id,tsk) = ptr;
				break;
			case SALLOC_THR:
				GET_PTR(id,thr) = ptr;
				break;
			case SALLOC_SMO:
				GET_PTR(id,smo) = ptr;
				break;
			default:
				break;		
		}

		if(ptr != NULL)
			ammount[type]++;

		return ptr;
	}
	else
	{
		// messages and ports wont have an ID
		void *ptr = csalloc(SALLOC2CONT_TYPE(type));	

		if(ptr != NULL)
			ammount[type]++;
		return ptr;
	}
}

/*
Free a given structure.
Ptr must be NOT NULL *only* if type is not THR, TSK or SMO.
*/
void sfree(void *ptr, int id, int type)
{
	if(type == SALLOC_TSK || type == SALLOC_THR || type == SALLOC_SMO)
	{
		if((type == SALLOC_TSK && !TST_PTR(id,tsk)) || (type == SALLOC_THR && 
			!TST_PTR(id,thr)) || (type == SALLOC_SMO && !TST_PTR(id,smo)))
			return;			// object non allocated

		switch(type)
		{
			case SALLOC_TSK:
				csfree(GET_PTR(id,tsk), SALLOC2CONT_TYPE(type));
				break;
			case SALLOC_THR:
				csfree(GET_PTR(id,thr), SALLOC2CONT_TYPE(type));
				break;
			case SALLOC_SMO:
				csfree(GET_PTR(id,smo), SALLOC2CONT_TYPE(type));
				break;
			default:
				break;		
		}
		
		index_free(id, ALLOC2IDX(type));
	}
	else
	{
		csfree(ptr, SALLOC2CONT_TYPE(type));
	}
	ammount[type]--;
}

