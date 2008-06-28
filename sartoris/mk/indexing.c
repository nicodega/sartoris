

#include "sartoris/kernel.h"
#include "sartoris/kernel-data.h"
#include "lib/indexing.h"
#include "lib/containers.h"

struct index_dir idx_dir;

// Contains how many indexes we need for preallocated structures
#define IDX_STATIC_THR_COUNT   IDX_LEN(CONT_STATIC_THR * CONT_THR_PER_CONT)
#define IDX_STATIC_TSK_COUNT   IDX_LEN(CONT_STATIC_TSK * CONT_TSK_PER_CONT)
#define IDX_STATIC_SMO_COUNT   IDX_LEN(CONT_STATIC_SMO * CONT_SMO_PER_CONT)

// How many indexes we need in total
#define IDX_THR_COUNT   IDX_LEN(MAX_THR)
#define IDX_TSK_COUNT   IDX_LEN(MAX_TSK)
#define IDX_SMO_COUNT   IDX_LEN(MAX_SMO)

// Pre allocated pages (in bytes) for indexes.
#define IDX_PREALLOC_BYTES ((IDX_STATIC_THR_COUNT + IDX_STATIC_TSK_COUNT + IDX_STATIC_SMO_COUNT) * PG_SIZE)
// Returns true, if address is pre allocated, false otherwise
#define IDX_PREALLOC(addr) ((unsigned int)addr < (IDX_PREALLOC_BYTES + (unsigned int)static_idx_mem))

/* Initial preallocated indexes */
unsigned char static_idx_mem [IDX_PREALLOC_BYTES] __attribute__ ((aligned (PG_SIZE))); 

void init_indexes()
{
	int id = 0;

	/*
	Initially all indexes will be non allocated (except preallocated)
	*/

	for(id = 0; id < IDX_TSK_COUNT; id++)
	{
		idx_dir.index_ptr_tsk[id] = (id < IDX_STATIC_TSK_COUNT)? (void*)((unsigned int)static_idx_mem + id * PG_SIZE) : NULL;
		idx_dir.free_ptrs_tsk[id] = IDX_SIZE;
	}

	for(id = 0; id < IDX_THR_COUNT; id++)
	{
		idx_dir.index_ptr_thr[id] = (id < IDX_STATIC_THR_COUNT)? (void*)((unsigned int)static_idx_mem + IDX_STATIC_TSK_COUNT * PG_SIZE + id * PG_SIZE) : NULL;
		idx_dir.free_ptrs_thr[id] = IDX_SIZE;
	}

	for(id = 0; id < IDX_SMO_COUNT; id++)
	{
		idx_dir.index_ptr_smo[id] = (id < IDX_STATIC_SMO_COUNT)? (void*)((unsigned int)static_idx_mem + (IDX_STATIC_THR_COUNT + IDX_STATIC_TSK_COUNT) * PG_SIZE + id * PG_SIZE) : NULL;
		idx_dir.free_ptrs_smo[id] = IDX_SIZE;
	}
}

/*
Allocate the index page for a given id.
returns 1 if successful, 0 otherwise.
*/
int index_alloc(int id, int type)
{
	/* check if we have index page allocated */
	switch(type)
	{
		case IDX_TSK:
			if(!TST_DIR_PTR(id,tsk))
			{
				// we will set index entry to a bogus value
				// because dynamic allocation might break 
				// atomicity
				SET_DIR_PTR(id,tsk,(struct index *)dyn_alloc_page(DYN_PGLVL_IDX));
				if(!TST_DIR_PTR(id,tsk))
					return 0;
				SET_DIR_FREE(id,tsk,IDX_SIZE-1);
			}
			else
			{
				SET_DIR_FREE(id,tsk,GET_DIR_FREE(id,tsk)-1);
			}
			break;
		case IDX_THR:
			if(!TST_DIR_PTR(id,thr))
			{
				SET_DIR_PTR(id,thr,(struct index *)dyn_alloc_page(DYN_PGLVL_IDX));
				if(!TST_DIR_PTR(id,thr))
					return 0;						
				SET_DIR_FREE(id,thr,IDX_SIZE-1);
			}
			else
			{
				SET_DIR_FREE(id,thr,GET_DIR_FREE(id,thr)-1);
			}
			break;
		case IDX_SMO:
			if(!TST_DIR_PTR(id,smo))
			{
				SET_DIR_PTR(id,smo,(struct index *)dyn_alloc_page(DYN_PGLVL_IDX));
				if(!TST_DIR_PTR(id,smo)) 
					return 0;
				SET_DIR_FREE(id,smo,IDX_SIZE-1);
			}
			else
			{
				SET_DIR_FREE(id,smo,GET_DIR_FREE(id,smo)-1);
			}
			break;
	}

	return 1;
}

/*
Set the index as NULL. 
Free the index page if necesary.
TODO: Add delayed index free.
*/
void index_free(int id, int type)
{
	int *free;
	void *idx;

	switch(type)
	{
		case IDX_TSK:
			if(!TST_PTR(id,tsk)) return;
			
			SET_PTR(id,tsk,NULL);
			free = &GET_DIR_FREE(id,tsk);
			idx = &GET_DIR_PTR(id,tsk);
			break;
		case IDX_THR:
			if(!TST_PTR(id,thr)) return;
			
			SET_PTR(id,thr,NULL);
			free = &GET_DIR_FREE(id,thr);
			idx = &GET_DIR_PTR(id,tsk);
			break;
		case IDX_SMO:
			if(!TST_PTR(id,smo)) return;
			
			SET_PTR(id,smo,NULL);
			free = &GET_DIR_FREE(id,smo);
			idx = &GET_DIR_PTR(id,tsk);
			break;
	}

	(*free)++;

	if(*free == IDX_SIZE && !IDX_PREALLOC(idx))
	{
		/* Return index page to OS */
		dyn_free_page(idx, DYN_PGLVL_IDX);
		switch(type)
		{
			case IDX_TSK:
				SET_DIR_PTR(id,tsk,NULL);
				break;
			case IDX_THR:
				SET_DIR_PTR(id,thr,NULL);
				break;
			case IDX_SMO:
				SET_DIR_PTR(id,smo,NULL);
				break;
		}
	}
}

/*
Find a free index for the given type.
NOTE: Right now, this is far from optimal.
*/
int index_find_free(int type)
{
	int i = 0, j = -1, c = -1;
	switch(type)
	{
		case IDX_TSK:
			break;
		case IDX_THR:
			break;
		case IDX_SMO:
			// only smos will be given an id by sartoris
			// First try to find an allocated directory entry
			for(i = 0; i < IDX_SMO_COUNT; i++)
			{
				if(idx_dir.index_ptr_smo[i] != NULL)
				{
					if(idx_dir.free_ptrs_smo[i] > 0)
					{
						/* Found and allocated directory with a free id */
						for(j = 0; j < IDX_SIZE; j++)
						{
							if(idx_dir.index_ptr_smo[i]->ptrs[j] == NULL)
								return i * IDX_SIZE + j;
						}
					}
				}
				else
				{
					// We have a candidate non-allocated index
					c = i;
				}
			}
			// We did not find an allocated index with a free id...
			// Return the lowest non allocated index ID
			j = c * IDX_SIZE;
			break;
	}
	return j;
}



