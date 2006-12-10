
#include "sartoris/kernel.h"

#ifndef _INDEXING_
#define _INDEXING_

struct index_dir
{
	struct index *index_ptr_thr[MAX_THR / (PG_SIZE / 4)];
	int free_ptrs_thr[MAX_THR / (PG_SIZE / 4)];

	struct index *index_ptr_tsk[MAX_TSK / (PG_SIZE / 4)];
	int free_ptrs_tsk[MAX_TSK / (PG_SIZE / 4)];

	struct index *index_ptr_smo[MAX_SMO / (PG_SIZE / 4)];
	int free_ptrs_smo[MAX_SMO / (PG_SIZE / 4)];
};

struct index
{
	void *ptrs[(PG_SIZE / 4)];  // <- pointers to our task / thread / smo  struct
};

/* In order to make this faster to translate, these are equal to CONT_ALLOC_XXX */
#define IDX_TSK CONT_ALLOC_TSK
#define IDX_THR CONT_ALLOC_THR
#define IDX_SMO CONT_ALLOC_SMO

#define ALLOC2IDX(type) (type)

/*
This will be our directory
*/
extern struct index_dir idx_dir;

/*
Using this directory aproach, we will hace to calculate given an id the following.
NOTE: ARCH_FAST_ID and ARCH_FAST_IDREM come from kernel-arch.h
*/
#define GET_ID_DIR(id) (ARCH_FAST_ID(id))
#define GET_ID_IDX(id) (ARCH_FAST_IDREM(id))

/*
Get a task/thread/smo pointer
*/
#define GET_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)])

/*
Defines for clearing a pointer to NULL;
*/
#define CLR_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)]=NULL;idx_dir.free_ptrs_ ## idxType[GET_ID_DIR(id)]++;)

/*
Defines for testing a given ID directory presence
*/
#define TST_DIR_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)] != NULL)
#define GET_DIR_PTR(id,idxType) (idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)])
#define GET_DIR_FREE(id,idxType) (idx_dir.free_ptrs_ ## idxType[GET_ID_DIR(id)])

/*
Defines for testing a given ID presence
*/
#define TST_PTR(id,idxType) (TST_DIR_PTR(id,idxType) && idx_dir.index_ptr_ ## idxType[GET_ID_DIR(id)]->ptrs[GET_ID_IDX(id)] !=NULL)


void init_indexes();
int index_alloc(int id, int type);
void index_free(int id, int type);
int index_find_free(int type);

#endif
