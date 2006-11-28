/*  
 *   Sartoris 0.5 shared memory objects management header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#ifndef _SHARED_MEM_H_
#define _SHARED_MEM_H_

#include <sartoris/kernel.h>
#include <sartoris/cpu-arch.h>

struct smo {
  int base;
  int len;
  int rights;
  int owner;
  int target;
  int valid;
  int next;
  int prev;
};

struct smo_container {
  struct smo smos[MAX_SMO];
  int task_first_smo[MAX_TSK];
  int task_num_smo[MAX_TSK];
  int free_first;
};

void init_smos(struct smo_container *smoc);

int get_new_smo(struct smo_container *smoc, int task, int target_task, int addr, int size, int perms);

int delete_smo(struct smo_container *smoc, int curr_task, int id);

void delete_task_smo(struct smo_container *smoc, int task_id);

#endif
