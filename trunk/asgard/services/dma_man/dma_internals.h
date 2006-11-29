/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

#ifndef DMA_INT
#define DMA_INT

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <services/dma_man/dma_man.h>
#include <drivers/dma/dma.h>
#include <os/pman_task.h>
#include <lib/malloc.h>
#include <lib/const.h>
#include <services/stds/stdservice.h>
#include <services/directory/directory.h>
#include <lib/structures/list.h>

#define BUFF_SIZE 0x400 // 1kb (will be used only for the memory pages list)
#define PAGE_BUFF_SIZE	0x70000	// 518kb will be the size of the paged buffer (64k x 7).

/* This structure will be used for assigning memory to channels 
(each page will be 64kb aligned)
*/
struct assigned_memory
{
	unsigned int start;
	unsigned int end;
	unsigned int size;
};

struct channel 
{
  int task;
  CPOSITION assigned_it;
  int smo;
};

void process_query_interface(struct stdservice_query_interface * service_cmd, int taskid);

void init_blocks();
CPOSITION get_buffer(unsigned int size, int align128);
void free_buffer(CPOSITION assigned_it);

#endif

