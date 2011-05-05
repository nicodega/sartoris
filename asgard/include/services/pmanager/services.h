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

#ifndef __PMAN_HEADER
#define __PMAN_HEADER

#define PMAN_COMMAND_PORT  2

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

/* message types */

#define PM_CREATE_TASK      0x01
#define PM_CREATE_THREAD    0x02
#define PM_DESTROY_TASK     0x10
#define PM_DESTROY_THREAD   0x11
#define PM_BLOCK_THREAD     0x20
#define PM_UNBLOCK_THREAD   0x21 
#define PM_SHUTDOWN		    0x22 
#define PM_FMAP			    0x23
#define PM_FMAP_FINISH	    0x24
#define PM_FMAP_FLUSH	    0x25
#define PM_PMAP_CREATE	    0x26
#define PM_PMAP_REMOVE	    0x27
#define PM_SHARE_MEM	    0x28
#define PM_UNSHARE_MEM	    0x29
#define PM_MMAP_CREATE	    0x30
#define PM_MMAP_REMOVE	    0x31
#define PM_INITIALIZING	    0x32
#define PM_PHYMEM           0x33
#define PM_TASK_FINISHED    0xFF

/* flags for task creation */

#define SERVER_TASK 0x0001

/* flags for thread creation */

#define INT_HANDLER 0x0001

/* error codes */

#define PM_OK                           0x0000
#define PM_TASK_ID_INVALID              0x0001
#define PM_TASK_ID_TAKEN                0x0002
#define PM_TASK_RETRY_LATER             0x0005
#define PM_IO_ERROR                     0x0006
#define PM_INVALID_FILEFORMAT           0x0008
#define PM_SHUTINGDOWN                  0x0009
#define PM_BAD_SMO                      0x0010
#define PM_NOT_ENOUGH_MEM               0x0011
#define PM_INVALID_TASK                 0x0012
#define PM_NO_FREE_ID                   0x0012
#define THR_CREATE_INT_NOTALLOWED       0x0013
#define THR_CREATE_INT_ALREADY_HANDLED  0x0014
#define THR_CREATE_MK_FAILED            0x0015
#define THR_CREATE_MK_INT_FAILED        0x0016
#define PM_INVALID_PARAMS               0x0017
#define PM_TOO_MANY_FILES_OPENED        0x0018
#define PM_MEM_COLITION                 0x0019
#define PM_ERROR                        0x0020


#define PM_THREAD_OK          0x0000
#define PM_THREAD_ID_INVALID  0x0001
#define PM_THREAD_NO_ROOM     0x0002
#define PM_THREAD_FAILED      0x0003
#define PM_THREAD_INT_TAKEN   0x0004

struct pm_msg_generic {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  char padding[10];
} PACKED_ATT;

struct pm_msg_phymem {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  unsigned int linear;
  char padding[6];
} PACKED_ATT;

struct pm_msg_phymem_response {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned int  physical;
  char padding[8];
} PACKED_ATT;

struct pm_msg_create_task {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short flags;
  short new_task_id;
  short path_smo_id;
  int param;
} PACKED_ATT;

struct pm_msg_create_thread {
  unsigned char pm_type;
  unsigned char int_priority;
  short req_id;
  short response_port;
  short task_id;
  short flags;
  short interrupt;
  void * entry_point;
} PACKED_ATT;

struct pm_msg_destroy_task {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  int ret_value;
  short task_id;
  short padding[2];
} PACKED_ATT;

struct pm_msg_destroy_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short thread_id;
  short padding[4];
} PACKED_ATT;

struct pm_msg_block_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short thread_id;
  short block_id;
  short padding[3];
} PACKED_ATT;

struct pm_msg_unblock_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short thread_id;
  short block_id;
  short padding[3];
} PACKED_ATT;



struct pm_msg_response {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  int   status;
  int   new_id;
  int   new_id_aux;
} PACKED_ATT;

/* PM_TASK_FINISHED */
struct pm_msg_finished {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short padding1;
  int ret_value;
  int taskid;
  short padding2;
} PACKED_ATT;

/* MMAP */
struct pm_msg_fmap 
{
  unsigned char pm_type;
  unsigned char padding;
  short req_id;
  short response_port;
  unsigned int params_smo;
  unsigned char padding1[6];
} PACKED_ATT;

struct fmap_params
{
	short fs_service;
	unsigned int fileid;
	unsigned int length;
	unsigned int perms;
	unsigned int addr_start;
	unsigned int offset;
} PACKED_ATT;

struct pm_msg_fmap_finish 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short padding;
  void *address;
  int padding1;
} PACKED_ATT;

struct pm_msg_fmap_flush 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  short response_port;
  short padding;
  void *address;
  int padding1;
} PACKED_ATT;

/* Physical memory mapping */
struct pm_msg_pmap_create	// PM_PMAP_CREATE
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;
  unsigned int start_phy_addr;
  unsigned short length; 
  unsigned char perms;
} PACKED_ATT;

#define PM_PMAP_EXCLUSIVE 0x1
#define PM_PMAP_WRITE	  0x2

struct pm_msg_pmap_remove   // PM_PMAP_REMOVE
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;	// linear address on task
  char padding[7];
} PACKED_ATT;

/* Memory sharing */
struct pm_msg_share_mem   // PM_SHARE_MEM
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;
  unsigned short perms;
  unsigned short length; 
  unsigned char padding[3];
} PACKED_ATT;

#define PM_SHARE_MEM_WRITE	0x1
#define PM_SHARE_MEM_READ	0x2

struct pm_msg_share_mem_response {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  int   status;
  int   shared_mem_id;
  int   new_id_aux;
} PACKED_ATT;

struct pm_msg_share_mem_remove   // PM_UNSHARE_MEM
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;	// linear address on task
  char padding[7];
} PACKED_ATT;

/* Shared memory mapping */
struct pm_msg_mmap			   // PM_MMAP_CREATE
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;
  unsigned int shared_mem_id;
  unsigned short length;
  unsigned char perms;
} PACKED_ATT;

struct pm_msg_mmap_remove     // PM_MMAP_REMOVE
{
  unsigned char pm_type;
  short req_id;
  short response_port;
  unsigned int start_addr;	// linear address on task
  char padding[7];
} PACKED_ATT;

/* Exception return values */
/* (Return values sent to task creator) */
#define DIV_ZERO_RVAL		-2
#define OVERFLOW_RVAL		-3
#define BOUND_RVAL			-4
#define INV_OPC_RVAL		-5
#define DEV_NOT_AV_RVAL		-6
#define STACK_FAULT_RVAL	-7
#define GEN_PROT_RVAL		-8
#define PAGE_FAULT_RVAL		-9
#define FP_ERROR_RVAL		-10
#define ALIG_CHK_RVAL		-11
#define SIMD_FAULT_RVAL		-12

#define PG_IO_ERROR         -13     // Task Was terminated due to 
                                    //an IO error while trying to fetch a page from storage device
#define PG_RW_ERROR         -14     // Tried to write on a read only page
#define MAXADDR_ERROR       -15     // tried to read/write on an address too high
#define FMAP_IO_ERROR		-16
#define NOT_ENOUGH_MEMORY	-17
#define PMAN_RULEZ			-18

#endif /* __PMAN_HEADER */
