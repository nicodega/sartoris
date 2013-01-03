
#include "sartoris/kernel.h"

#ifndef _INDEXING_
#define _INDEXING_

// Ammount of ids on an index
#define IDX_SIZE (PG_SIZE / 4)

// Returns ammount of indexes needed for the specified maximum ID
#define IDX_LEN(maxID) ( ((maxID) + (IDX_SIZE - ((maxID) % IDX_SIZE)) ) / IDX_SIZE )

/* Index directory */
struct index_dir
{
	struct index *index_ptr_thr[IDX_LEN(MAX_THR)];
	int free_ptrs_thr[IDX_LEN(MAX_THR)];

	struct index *index_ptr_tsk[IDX_LEN(MAX_TSK)];
	int free_ptrs_tsk[IDX_LEN(MAX_TSK)];

	struct index *index_ptr_smo[IDX_LEN(MAX_SMO)];
	int free_ptrs_smo[IDX_LEN(MAX_SMO)];
};

/* Index */
struct index
{
	void *ptrs[IDX_SIZE];  // <- pointers to our task / thread / smo  struct
};

/* In order to make this faster to translate, these are equal to CONT_ALLOC_XXX */
#define IDX_TSK CONT_ALLOC_TSK
#define IDX_THR CONT_ALLOC_THR
#define IDX_SMO CONT_ALLOC_SMO

#define ALLOC2IDX(type) (type)		// Translate to index type the container type

/*
This will be our directory.
*/
extern struct index_dir idx_dir;

/*
Using this directory aproach, we will have to calculate given an id, its index on directory and on 
its index.
This performs id / (PG_SIZE / 4) and id % (PG_SIZE / 4)
*/
#define GET_ID_DIR(id) (id >> 10)
#define GET_ID_IDX(id) (id & 0x3FF)

#define PTRtsk (struct task*)
#define PTRsmo (struct smo*)
#define PTRthr (struct thread*)

/*
Get a task/thread/smo pointer from the index
*/
#define GET_PTR(id,idxType) (PTR ## idxType idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)])

#define SET_PTR(id,idxType,val) idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)] = val

/*
Defines for testing a given ID directory presence
*/
#define TST_DIR_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)] != NULL)
#define GET_DIR_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)])
#define GET_DIR_FREE(id,idxType) (idx_dir.free_ptrs_ ## idxType[GET_ID_DIR(id)])
#define SET_DIR_PTR(id,idxType,val) idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)] = val
#define SET_DIR_FREE(id,idxType,val) idx_dir.free_ptrs_ ## idxType[GET_ID_DIR(id)] = val


/*
Defines for testing a given ID presence
*/
#define TST_PTR(id,idxType) (TST_DIR_PTR(id,idxType) && idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)] != NULL)


void init_indexes();
int index_alloc(int id, int type);
void index_free(int id, int type);
int index_find_free(int type);

#endif
