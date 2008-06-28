

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
extern struct _containers containers;
void *salloc(int id, int type)
{	
	if(ammount[type] == max_ammount[type]) 
		return NULL;
		
	if(type == SALLOC_TSK || type == SALLOC_THR || type == SALLOC_SMO)
	{		
		switch(type)
		{
			case SALLOC_TSK:
				if(TST_PTR(id,tsk)) return NULL;
				break;
			case SALLOC_THR:				
				if(TST_PTR(id,thr)) return NULL;
				break;
			case SALLOC_SMO:
				if(TST_PTR(id,smo)) return NULL;
				break;
			default:
				break;		
		}

		// allocate index first, for csalloc is not always atomic
		if(!index_alloc(id, ALLOC2IDX(type)))
			return NULL;
		
		ammount[type]++; // increment now, for atomicity might be broken on csalloc

		void * ptr = csalloc(SALLOC2CONT_TYPE(type)); 

		if(ptr == NULL)
		{
			k_scr_print("Alloc3",0x7);
			ammount[type]--;
			index_free(id, ALLOC2IDX(type));
			return NULL;
		}

		switch(type)
		{
			case SALLOC_TSK:
				SET_PTR(id,tsk,ptr);
				break;
			case SALLOC_THR:
				SET_PTR(id,thr,ptr);
				break;
			case SALLOC_SMO:
				SET_PTR(id,smo,ptr);
				break;
			default:
				break;		
		}

		return ptr;
	}
	else
	{
		// messages and ports wont have an ID
		ammount[type]++; // increment now, for atomicity might be broken on csalloc

		void *ptr = csalloc(SALLOC2CONT_TYPE(type));	

		if(ptr == NULL)
			ammount[type]--;

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
		switch(type)
		{
			case SALLOC_TSK:
				if(!TST_PTR(id,tsk)) return; // not allocated
				csfree(GET_PTR(id,tsk), SALLOC2CONT_TYPE(type));
				break;
			case SALLOC_THR:
				if(!TST_PTR(id,thr)) return;
				csfree(GET_PTR(id,thr), SALLOC2CONT_TYPE(type));
				break;
			case SALLOC_SMO:
				if(!TST_PTR(id,smo)) return;
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

