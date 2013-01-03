/*  
 *   Sartoris 0.5 shared memory objects management header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#ifndef _SHARED_MEM_H_
#define _SHARED_MEM_H_

#include <sartoris/kernel.h>

#define MAX_TSK_SMO   32

struct smo 
{
	int id;
	int base;
	int len;
	int rights;
	int owner;
	int target;
	struct smo *next;
	struct smo *prev;
};

int get_new_smo(int task, int target_task, int addr, int size, int perms);
int delete_smo(int id, int curr_task);
void delete_task_smo(int task_id);

#endif
