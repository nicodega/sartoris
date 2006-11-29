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

#ifndef RAMDISKINT
#define RAMDISKINT

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <os/layout.h>
#include <services/stds/stdservice.h>
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/directory/directory.h>
#include <lib/scheduler.h>
#include <lib/malloc.h>

///// DEFINES /////
#define DISK_SIZE 	0x200000 // Max disk size 2MB
#define RAMDISK_STDDEVMAX 10
#define MAX_OWNERS 10
#define STDDEV_BLOCK_DEV_PORT 2
#define STDDEV_PORT 1
	
/////// RET VALUES /////
#define RAMDISK_OK		0
#define RAMDISK_ERR		-1


#define RESET_SEEK 5
///// FUNCTIONS /////

int read_block(int block, int smo_id_dest);
int write_block(int block, int smo_id_src);

void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
struct stddev_res process_stddev(struct stddev_cmd stddev_msg, int sender_id);
void swap_array(int *array, int start, int length);
int check_ownership(int process_id, int logic_device);
int mwrite_block(struct stdblockdev_writem_cmd *cmd);
int mread_block(struct stdblockdev_readm_cmd *cmd);

int ramdisk_rw(int block, int smo_id, int size, int write);
void init_disk(char *disk);
#endif

