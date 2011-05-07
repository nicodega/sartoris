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


#include "types.h"

#ifndef PMANIOH
#define PMANIOH


struct pm_thread;
struct pm_task;

/* IO Init Stages */
#define IO_STAGE_WAITATAC 0
#define IO_STAGE_FINDDEV  1
#define IO_STAGE_WAITOFS  2
#define IO_STAGE_MOUNROOT 3
#define IO_STAGE_WAITATACSWP 4
#define IO_STAGE_SWAPDEV  5
#define IO_STAGE_FINISHED 6

/* This must be designed independently from the IO event source */
#define FILE_IO_TASK	1	// IO Source belongs to a task.
#define FILE_IO_THREAD	2	// IO Source belongs to a Thread.
#define FILE_IO_FMAP	3	// IO Source belongs to a File Mapping.

/* Open modes for Files */
#define IO_OPEN_MODE_READ	STDFSS_FILEMODE_READ
#define IO_OPEN_MODE_WRITE	STDFSS_FILEMODE_WRITE

/* File System IO Required event source variables */
struct fsio_event_source
{
	UINT16	type;			/* FILE_IO_XXXX      */
	UINT16	id;				/* Object ID (depending on type) */
	INT32	file_id;		/* File Handle asociated with source */
	INT32	fs_service;		/* File System Service Task ID (must be STDFSS compliant) */
	INT32	smo;			/* Shared Memory Object used for read/write operation */
	UINT32	size;			/* File Size */
	UINT32	port;			/* PMAN IO Port used */
	UINT32	flags;			
} PACKED_ATT;

#define IOEVT_FLAG_WAITING_RESPONSE		1	// This source is waiting for an FS response (blocked)

/* 
A callback event for File System IO events. 
*/
struct io_event
{
	/* Function being invoked */
	INT32 (*callback)(struct fsio_event_source *iosrc, INT32 ioret);	// The callback function
	INT32 params[2];													// Parameters for the event
} PACKED_ATT;

/* 
Swap Partition event for reading (used on threads)
*/
struct swp_io_evt
{
	UINT32 (*callback)(struct pm_thread *thread, UINT32 ioret);
} PACKED_ATT;

/* 
Swap Partition event for reading (used on tasks) 
*/
struct tsk_swp_io_evt
{
	UINT32 (*callback)(struct pm_task *task, UINT32 ioret);
	INT32 params[2];													// Parameters for the event
} PACKED_ATT;

/* IO return values for Callbacks */
#define IO_RET_OK 0
#define IO_RET_ERR -1

/* 
A Structure used for writing to swap partition/filesystem, using ioslots. (request is not attached to an object)
*/
struct io_slot;

struct ioslot_evt
{
	UINT32 (*callback)(struct io_slot *ioslot, UINT32 ioret);
} PACKED_ATT;

struct swap_dest
{
	UINT32 swap_addr;	// Swap Address asigned to each IO slot in addr	
	UINT32 padding;		
} PACKED_ATT;

struct fs_dest
{
	UINT32 task_id;
	UINT32 file_id;
};

#define IOSLOT_TYPE_FREE         0
#define IOSLOT_TYPE_FS           1
#define IOSLOT_TYPE_SWAP         2
#define IOSLOT_TYPE_INVALIDATED  0xFF

#define IOSLOT_OWNER_TYPE_TASK   0

/* An IO slot */
struct io_slot
{
	BYTE type;
	BYTE owner_type;
	UINT32 pg_address;		// Linear address for page being written
	union 
	{
		struct swap_dest swap;
		struct fs_dest fs;
	} dest;					// Filesystem information/Swap information
	UINT32 smo;				// Shared memory object used for the operation
	struct ioslot_evt finished;		// Callback function invoked upon completion
	struct io_slot *next_free;		// Next IO Slot free
	UINT16 task_id;
} PACKED_ATT;

#define PMAN_IOSLOTS 40

/*
IO Slots container
*/
struct io_slots
{
	struct io_slot *free_first;	
	struct io_slot slots[PMAN_IOSLOTS];
	UINT32 free_count;
} PACKED_ATT;

struct io_slots ioslots;

/*
Returns Task ID of the OFSS
*/
UINT16 io_get_stdfs_process();

/* Struct Initializers */
void swp_io_init(struct swp_io_evt *evt);
void io_init_event(struct io_event *evt, struct fsio_event_source *iosrc);
void io_init_source(struct fsio_event_source *iosrc, UINT16 type, UINT16 id);

/* IO Subsystem Initialization */
void io_begin_init();
void io_swap_init();
BOOL io_initialized();

/* IO Message Processing */
void io_process_msg();

/* IO Commands */

/*
Begins an open operation for the event source.
When completed, the source callback function will be invoked.
*/
BOOL io_begin_open(struct fsio_event_source *iosrc, char *filename, UINT32 flen, INT32 mode);
/*
Begins a read Operation for the event source.
When completed, the source callback function will be invoked.
*/
INT32 io_begin_read(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr);
/*
Begins a Write Operation for the event source.
When completed, the source callback function will be invoked.
*/
INT32 io_begin_write(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr);
/*
Begins a seek Operation for the event source.
When completed, the source callback function will be invoked.
*/
INT32 io_begin_seek(struct fsio_event_source *iosrc, UINT32 offset);
/*
Begins a close Operation for the event source.
When completed, the source callback function will be invoked.
*/
BOOL io_begin_close(struct fsio_event_source *iosrc);
/*
Begins a Read operation from ATAC. (Thread Based) 
When completed, the Thread swp_io_finished callback function will be invoked.
*/
BOOL io_begin_pg_read(UINT32 lba, ADDR ptr, struct pm_thread *thr);
/*
Begins a Read operation from ATAC. (Task Based) 
When completed, the Task swp_io_finished callback function will be invoked.
*/
BOOL io_begin_task_pg_read(UINT32 lba, ADDR ptr, struct pm_task *task);
/*
Begin an IO Slot write operation.
Whe finished, IO Slot callback will be invoked (from PMAN scheduler thread).
*/
BOOL io_slot_begin_write(UINT32 possition, ADDR ptr, UINT32 ioslot_id);
/*
Get a Free IO Slot ID.
Returns 0xFFFFFFFF if there are no IO Slots available.
*/
UINT32 ioslot_get_free();
/*
Return an ioslot to the free pool.
*/
void ioslot_return(UINT32 id);
/*
Begin a Takeover operation on STDFSS (used for FMAPS)
Upon completion the source callback function will be invoked.
*/
BOOL io_begin_takeover(struct fsio_event_source *iosrc, UINT32 fileid, ADDR fmap_desc, UINT16 task);
/*
Begin a Release operation on STDFSS (used for FMAPS)
Upon completion the source callback function will be invoked.
*/
BOOL io_begin_release(struct fsio_event_source *iosrc);



#endif
