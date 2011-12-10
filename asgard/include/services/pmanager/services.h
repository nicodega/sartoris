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

#define PM_CREATE_TASK      1
#define PM_CREATE_THREAD    2
#define PM_DESTROY_TASK     3
#define PM_DESTROY_THREAD   4
#define PM_BLOCK_THREAD     5
#define PM_UNBLOCK_THREAD   6 
#define PM_SHUTDOWN		    7 
#define PM_FMAP			    8
#define PM_FMAP_FINISH	    9
#define PM_FMAP_FLUSH	    10
#define PM_PMAP_CREATE	    11
#define PM_PMAP_REMOVE	    12
#define PM_SHARE_MEM	    13
#define PM_UNSHARE_MEM	    14
#define PM_MMAP_CREATE	    15
#define PM_MMAP_REMOVE	    16
#define PM_INITIALIZING	    17
#define PM_PHYMEM           18
#define PM_LOAD_LIBRARY     19
#define PM_LOADER_READY     20
#define PM_DBG_ATTACH       21
#define PM_DBG_END          22
#define PM_DBG_TRESUME      23
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
#define PM_IS_INITIALIZING              0x001a
#define PM_INVALID_BOUNDARIES           0x001b
#define PM_ERROR                        0x001c
#define PM_ALREADY_DEBUGGING            0x001d
#define PM_NOT_DEBUGGING                0x001e
#define PM_NOT_BLOCKED                  0x001f
#define PM_NO_PERMISSION                0x0020
#define PM_INVALID_THREAD               0x0021
#define PM_ALREADY_BLOCKED              0x0022

#define PM_THREAD_OK                0x0000
#define PM_THREAD_ID_INVALID        0x0001
#define PM_THREAD_NO_ROOM           0x0002
#define PM_THREAD_FAILED            0x0003
#define PM_THREAD_INT_TAKEN         0x0004
#define PM_THREAD_INVALID_STACK     0x0005

struct pm_msg_generic 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding[11];
} PACKED_ATT;

struct pm_msg_dbgtresume
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  short thread_id;
  char padding[9];
} PACKED_ATT;

struct pm_msg_dbgattach 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short task_id;            // task to begin debugging
  short dbg_port;           // port to where debug messages will be sent (including when a break interrupt raises)
  char padding[7];
} PACKED_ATT;

struct pm_msg_dbgend
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short task_id;            // task being debugged
  char padding[7];
} PACKED_ATT;

struct pm_msg_loadlib 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short path_smo_id;
  unsigned int vlow;
  unsigned int vhigh;
} PACKED_ATT;

struct pm_msg_phymem {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
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
  unsigned char response_port;
  char padding1;
  short flags;
  short new_task_id;
  short path_smo_id;
  int param;
} PACKED_ATT;

struct pm_msg_create_thread {
  unsigned char pm_type;
  unsigned char int_priority;
  short req_id;
  unsigned char response_port;
  short task_id;
  unsigned char interrupt;
  void *stack_addr;
  void *entry_point;
} PACKED_ATT;

struct pm_msg_destroy_task {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short task_id;
  int ret_value;
  unsigned int padding;
} PACKED_ATT;

struct pm_msg_destroy_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short thread_id;
  short padding[4];
} PACKED_ATT;

#define THR_BLOCK       0
#define THR_BLOCK_MSG   1

struct pm_msg_block_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char block_type;                  // THR_BLOCK_MSG / THR_BLOCK
  short thread_id;
  unsigned int ports_mask;
  int pading;
} PACKED_ATT;

struct pm_msg_unblock_thread {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  short thread_id;
  int padding[2];
} PACKED_ATT;


struct pm_msg_response {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  int   status;
  int   new_id;
  int   new_id_aux;
} PACKED_ATT;

/* PMAN_LOAD_LIBRARY */
struct pm_msg_loadlib_res {
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  int   status;
  int   lib_base;
  int   padding;
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
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1;
  unsigned int params_smo;
  unsigned char padding2[6];
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
  unsigned char response_port;
  char padding1[7];
  void *address;
} PACKED_ATT;

struct pm_msg_fmap_flush 
{
  unsigned char pm_type;
  unsigned char padding0;
  short req_id;
  unsigned char response_port;
  char padding1[7];
  void *address;
} PACKED_ATT;

/* Physical memory mapping */
struct pm_msg_pmap_create	// PM_PMAP_CREATE
{
  unsigned char pm_type;
  unsigned char padding0;  
  short req_id;
  unsigned char response_port;
  unsigned char flags;
  unsigned short pages;         // size in pages the process wants to map
  unsigned int start_addr;          
  unsigned int start_phy_addr;  // can be 0xFFFFFFFF if the service does not want a 
                                // particular physical address but just a chunk of memory
} PACKED_ATT;

#define PM_PMAP_CACHEDIS  0x1   // disable cache for this region
#define PM_PMAP_WRITE	  0x2   // allow write to this area
#define PM_PMAP_IO   	  0x4   // this will be an area used for a device IO
#define PM_PMAP_LOW_MEM   0x8

struct pm_msg_pmap_remove   // PM_PMAP_REMOVE
{
  unsigned char pm_type;
  short req_id;
  unsigned char response_port;
  char safe;                // is it safe to give this physical pages to any proc? or is it stil IO mapped.
  unsigned int start_addr;	// linear address on task
  char padding[7];
} PACKED_ATT;

/* Memory sharing */
struct pm_msg_share_mem   // PM_SHARE_MEM
{
  unsigned char pm_type;
  short req_id;
  unsigned char response_port;
  char padding0;
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
  unsigned char response_port;
  char padding0;
  unsigned int start_addr;	// linear address on task
  char padding[7];
} PACKED_ATT;

/* Shared memory mapping */
struct pm_msg_mmap			   // PM_MMAP_CREATE
{
  unsigned char pm_type;
  short req_id;
  unsigned char response_port;
  char padding0;
  unsigned int start_addr;
  unsigned int shared_mem_id;
  unsigned short length;
  unsigned char perms;
} PACKED_ATT;

struct pm_msg_mmap_remove     // PM_MMAP_REMOVE
{
  unsigned char pm_type;
  short req_id;
  unsigned char response_port;
  char padding0;
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
#define DEBUG_RVAL   		-13

#define PG_IO_ERROR         -13     // Task Was terminated due to 
                                    //an IO error while trying to fetch a page from storage device
#define PG_RW_ERROR         -14     // Tried to write on a read only page
#define MAXADDR_ERROR       -15     // tried to read/write on an address too high
#define FMAP_IO_ERROR		-16
#define NOT_ENOUGH_MEMORY	-17
#define PMAN_RULEZ			-18

#endif /* __PMAN_HEADER */
