

#include "sartoris/kernel.h"
#include "lib/indexing.h"
#include "lib/containers.h"

struct index_dir idx_dir;

#define IDX_STATIC_THR   ((CONT_STATIC_THR * CONT_THR_PER_CONT) * 4 + (CONT_STATIC_THR * CONT_THR_PER_CONT) * 4 % PG_SIZE )
#define IDX_STATIC_TSK   ((CONT_STATIC_TSK * CONT_TSK_PER_CONT) * 4 + (CONT_STATIC_TSK * CONT_TSK_PER_CONT) * 4 % PG_SIZE )
#define IDX_STATIC_SMO   ((CONT_STATIC_SMO * CONT_SMO_PER_CONT) * 4 + (CONT_STATIC_SMO * CONT_SMO_PER_CONT) * 4 % PG_SIZE )

#define IDX_STATIC_THR_COUNT  (IDX_STATIC_THR / PG_SIZE)
#define IDX_STATIC_TSK_COUNT  (IDX_STATIC_TSK / PG_SIZE)
#define IDX_STATIC_SMO_COUNT  (IDX_STATIC_SMO / PG_SIZE)

#define IDX_PREALLOC_BYTES (IDX_STATIC_THR + IDX_STATIC_TSK + IDX_STATIC_SMO)

#define IDX_PREALLOC(addr) ((unsigned int)addr < (IDX_PREALLOC_BYTES + (unsigned int)static_idx_mem))

/* Initial preallocated indexes */
unsigned char static_idx_mem [IDX_PREALLOC_BYTES] __attribute__ ((aligned (PG_SIZE))); 

void init_indexes()
{
	int id = 0;

	/*
	Initially all indexes will be non allocated (except preallocated)
	*/
	for(id = 0; id < (MAX_TSK >> 10); id++)
	{
		idx_dir.index_ptr_tsk[GET_ID_DIR(id)] = (id < IDX_STATIC_THR_COUNT)? (void*)((unsigned int)static_idx_mem + GET_ID_DIR(id) * PG_SIZE) : NULL;
		idx_dir.free_ptrs_tsk[GET_ID_DIR(id)] =  (PG_SIZE / 4);
	}

	for(id = 0; id < (MAX_THR >> 10); id++)
	{
		idx_dir.index_ptr_thr[GET_ID_DIR(id)] = (id < IDX_STATIC_TSK_COUNT)? (void*)((unsigned int)static_idx_mem + IDX_STATIC_THR + GET_ID_DIR(id) * PG_SIZE) : NULL;
		idx_dir.free_ptrs_thr[GET_ID_DIR(id)] = (PG_SIZE / 4);
	}

	for(id = 0; id < (MAX_SMO >> 10); id++)
	{
		idx_dir.index_ptr_smo[GET_ID_DIR(id)] = (id < IDX_STATIC_SMO_COUNT)? (void*)((unsigned int)static_idx_mem + IDX_STATIC_THR + IDX_STATIC_TSK + GET_ID_DIR(id) * PG_SIZE) : NULL;
		idx_dir.free_ptrs_smo[GET_ID_DIR(id)] = (PG_SIZE / 4);
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
#ifdef _DYNAMIC_
				GET_DIR_PTR(GET_ID_DIR(id), tsk) = dyn_alloc_page(DYN_PGLVL_IDX);
#endif
				if(!TST_DIR_PTR(id,tsk)) return 0;						
				GET_DIR_FREE(id,tsk) = (PG_SIZE / 4);
			}

			GET_DIR_FREE(id,tsk)--;
			break;
		case IDX_THR:
			if(!TST_DIR_PTR(id,thr))
			{
#ifdef _DYNAMIC_
				GET_DIR_PTR(GET_ID_DIR(id), thr) = dyn_alloc_page(DYN_PGLVL_IDX);
#endif
				if(!TST_DIR_PTR(id,thr)) return 0;						
				GET_DIR_FREE(id,thr) = (PG_SIZE / 4);
			}

			GET_DIR_FREE(id,thr)--;
			break;
		case IDX_SMO:
			if(!TST_DIR_PTR(id,smo))
			{
#ifdef _DYNAMIC_
				GET_DIR_PTR(GET_ID_DIR(id), smo) = dyn_alloc_page(DYN_PGLVL_IDX);
#endif
				if(!TST_DIR_PTR(id,smo)) return 0;						
				GET_DIR_FREE(id,smo) = (PG_SIZE / 4);
			}

			GET_DIR_FREE(id,smo)--;
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
			
			GET_PTR(id,tsk) = NULL;
			free = &GET_DIR_FREE(id,tsk);
			idx = &GET_DIR_PTR(id,tsk);
			break;
		case IDX_THR:
			if(!TST_PTR(id,thr)) return;
			
			GET_PTR(id,thr) = NULL;
			free = &GET_DIR_FREE(id,thr);
			idx = &GET_DIR_PTR(id,tsk);
			break;
		case IDX_SMO:
			if(!TST_PTR(id,smo)) return;
			
			GET_PTR(id,smo) = NULL;
			free = &GET_DIR_FREE(id,smo);
			idx = &GET_DIR_PTR(id,tsk);
			break;
	}

	(*free)++;

	if(*free == (PG_SIZE / 4) && !IDX_PREALLOC(idx))
	{
#ifdef _DYNAMIC_
		/* Return index page to OS */
		dyn_free_page(idx, DYN_PGLVL_IDX);
		switch(type)
		{
			case IDX_TSK:
				GET_DIR_PTR(id,tsk) = NULL;
				break;
			case IDX_THR:
				GET_DIR_PTR(id,tsk) = NULL;
				break;
			case IDX_SMO:
				GET_DIR_PTR(id,tsk) = NULL;
				break;
		}
#endif
	}
}

/*
Find a free index for the given type.
NOTE: Right now, this is far from optimal.
*/
int index_find_free(int type)
{
	int i = 0, j = -1;
	switch(type)
	{
		case IDX_TSK:
			break;
		case IDX_THR:
			break;
		case IDX_SMO:
			// only smos will be given an id by sartoris
			for(i = 0; i < (MAX_SMO / (PG_SIZE / 4)); i++)
			{
				if(TST_DIR_PTR(i,smo))
				{
					if(GET_DIR_FREE(i,smo) > 0)
					{
						/* Found and allocated directory with a free id */
						for(j = 0; j < (PG_SIZE / 4); j++)
						{
							if(idx_dir.index_ptr_smo[i]->ptrs[j] == NULL)
								return i * (PG_SIZE / 4) + j * 4;
						}
					}
				}
				else if(j == -1)
					j = i * (PG_SIZE / 4);
			}
			break;
	}
	return j;
}



