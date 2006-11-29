/*
*	Process and Memory Manager Service
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


#ifndef PAGEALLOCH
#define PAGEALLOCH

/* Struct used as a parameter to the page aging thread */
struct page_aging_t
{
	unsigned int pass;	// if 0 aging thread is on it's first pass. If 1, its on the second pass.
	unsigned int lb;	// lower bound
	unsigned int ub;	// lower bound
	unsigned int ac;	// age count (how many pages must be aged from this region)
	unsigned int finished;	// if 0 the aging thread has not finished working on this region
};

/* Struct used as a parameter to the page stealing thread */
struct page_stealing_t
{
	unsigned int lb;	// lower bound
	unsigned int ub;	// lower bound
	unsigned int sc;	// steal count (how many pages should be stolen from this region)

	unsigned int finished;		// if 0 the stealing thread has not finished working on this region
	unsigned int last_freed;	// address of the upper page freed (used for bound improving)
	unsigned int fc;			// how much pages have been freed
	unsigned int iterations;	
};

/* An structure used to keep track of pending IO on the stealing process */
#define PA_IOPENDING_FREEMASK 0x00010000       // this won't collide with addresses used on the free pool
#define PA_IOPENDING_FREE(x) (x & PA_IOPENDING_FREEMASK)
#define PA_IOPENDING_FREEINDEX(x) (x & ~PA_IOPENDING_FREEMASK)

#define PA_IOPENDING_SLOTS 256	// this is 256 because msg_id on std block dev is an unsigned char 
								// PA_IOPENDING_SLOTS * 2 + 1 cannot be bigger than 4096

struct page_stealing_iopending
{
	unsigned int free_first;	// first slot free on this structure. If value is > 1023 then there are no free slots.
								// each free address will contain the id on the addr array of the next free item.
	unsigned int addr[PA_IOPENDING_SLOTS];		// 512 addresses from physical memory which can be sent to io at the same time.
	unsigned int swap_addr[PA_IOPENDING_SLOTS];	// swap address asigned to each IO slot in addr	
};


/* A structure used to keep track of slots used on swap file */
struct page_stealing_swapbitmap
{
	unsigned int storage_pages;		// pages on storage
	unsigned int storage_free;		
	struct page_stealing_swapbitmap_storage *storage;
};

struct page_stealing_swapbitmap_storage
{
	unsigned char *addr[1024];		// the bitmap will be composed of up to 1024 pages
};


/* IO and swap variables */
extern unsigned int page_stealing_iocount;		// current io operations
extern unsigned int page_stealing_ldevid;		// logic device for swap file
extern unsigned int page_stealing_swappages;	// pages available on swapfile
extern unsigned int page_stealing_swapstart;	// first usable sector on swap file

/* Thread parameters */
extern struct page_aging_t pa_param;
extern struct page_stealing_t ps_param;

extern struct page_stealing_iopending *iopending;
extern struct page_stealing_swapbitmap swapbitmap;

/* Memory bounds */
extern unsigned int pa_en_mem;		// how much free memory is considered to be enough
extern unsigned int pa_des_mem;	// how much free memory is considered to be the desired ammount
extern unsigned int pa_min_mem;	// how much free memory is considered to be the minimum at witch threads are given high priority

extern unsigned int pa_curr_mem;	// how much free memory is available right now

extern unsigned int min_hits;

/* PA Sched variables */
/* Timing variables for stealing thread */
extern unsigned int pa_stealing_stop;
extern unsigned int pa_stealing_ticksspan;		// how many clock ticks will we wait until we run the thread again
extern unsigned int pa_stealing_tickscount;		// how many clock ticks will be granted to the thread
extern unsigned int pa_stealing_tickscurr;		// how many clock ticks have been granted to the thread so far

extern unsigned int pa_stealing_ticks;			// next scheduled ticks for this thread
extern int pa_stealing_direction;				// next scheduled ticks direction for this thread

/* Timing variables for aging thread */
extern unsigned int pa_aging_stop;
extern unsigned int pa_aging_ticksspan;		// how many clock ticks will we wait until we run the thread again
extern unsigned int pa_aging_tickscount;		// how many clock ticks will be granted to the thread
extern unsigned int pa_aging_tickscurr;		// how many clock ticks have been granted to the thread so far

extern unsigned int pa_aging_ticks;		// next scheduled ticks for this thread
extern int pa_aging_direction;				// next scheduled ticks direction for this thread

/* Bounds will be give for 1024 pages span, and the difference beween 
	aging thread lower bound and stealing thread upper bound will have to be less than 1/2 of 
	free mem, and greater than 1/16 free mem. */
extern unsigned int aging_stealing_maxdistance;	// max distance between memory areas for the two processes 
extern unsigned int aging_stealing_mindistance;	// min distance between memory areas for the two processes
extern unsigned int aging_stealing_currdistance;	// current distance between memory areas for the two processes

extern unsigned int pa_en_mem;		// how much free memory is considered to be enough
extern unsigned int pa_des_mem;	// how much free memory is considered to be the desired ammount
extern unsigned int pa_min_mem;	// how much free memory is considered to be the minimum at witch threads are given high priority

extern unsigned int pa_curr_mem;	// how much free memory is available right now

extern unsigned int min_hits;

/* Taken structure entries */
struct taken_pg
{
	unsigned int zero:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int tbl_index:10;
	unsigned int flags:16;
};

struct taken_ptbl
{
	unsigned int zero:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int dir_index:10;
	unsigned int taskid:16;
};

struct taken_pdir
{
	unsigned int zero:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int reserved:10;
	unsigned int taskid:16;
};

struct taken_entry
{
	union
    {
		unsigned int b;
		struct taken_pg b_pg;
		struct taken_ptbl b_ptbl;
		struct taken_pdir b_pdir;
    } data;
};

struct taken_table
{
	struct taken_entry entries[1024];
};

/* This structure will work as a page directory */
struct taken_entries
{
	struct taken_table *tables[1024];
};

/* The taken structure. (this is all the metadata we keep on each page) */
extern struct taken_entries *taken;

#define PA_MINHITS					3				// if min hits reaches this value, pa_des_mem will be decreased
#define PA_MEMPAROLE				(0x1000 * 20)
#define PA_DESMEMDEC				PA_MEMPAROLE
#define PA_DEFAULTSPAN				(200 / 4)		// 4 times a second. each tick is 5ms wide
#define PA_DEFAULT_TICKSCOUNT		1

/* Taken entry defines */
#define TAKEN_FLAG			0x1
#define TAKEN_PDIR_FLAG		0x2
#define TAKEN_PTBL_FLAG		0x4
#define TAKEN_SWPF_FLAG		0x8

#define SET_TAKEN(x)		((x)->data.b = (x)->data.b | TAKEN_FLAG)
#define SET_NOTTAKEN(x)		((x)->data.b = (x)->data.b & ~TAKEN_FLAG)

#define IS_TAKEN(x)			(((unsigned int)(x)->data.b & TAKEN_FLAG) == 1)
#define TAKEN_PDIR(x)		((unsigned int)(x)->data.b & TAKEN_PDIR_FLAG)
#define TAKEN_PTBL(x)		((unsigned int)(x)->data.b & TAKEN_PTBL_FLAG)

#define CREATE_TAKEN_DIR_ENTRY(tentry, taskid)					 {(tentry)->data.b = 0; (tentry)->data.b |= TAKEN_PDIR_FLAG | (taskid << 16);}
#define CREATE_TAKEN_TBL_ENTRY(tentry, taskid, dirindex, eflags) {(tentry)->data.b = 0; (tentry)->data.b |= TAKEN_PTBL_FLAG | (taskid << 16) | (dirindex << 6) | (eflags << 3);}
#define CREATE_TAKEN_PG_ENTRY(tentry, flags, index, eflags)		 {(tentry)->data.b = 0; (tentry)->data.b |= (flags << 16) | (index << 6) | (eflags << 3);}

#define TAKEN_PG_FLAG_LCK		0x1
#define TAKEN_PG_FLAG_SHARED	0x2
#define TAKEN_PG_FLAG_FILE		0x4
#define TAKEN_PG_FLAG_PMAN		0x8

/* eflags */
#define TAKEN_EFLAG_IOLOCK		0x1
#define TAKEN_EFLAG_SERVICE		0x2
#define TAKEN_EFLAG_PF			0x4

/* PMAN paging table entries */
#define PMAN_TBL_ENTRY_NULL 0x1
#define CREATE_PMAN_TBL_ENTRY(task_id, dir_index, flags) ( (unsigned int)0 | (task_id << 16) | (dir_index << 6) | (flags << 1))
#define PMAN_TBL_GET_TASK(entry) ((entry >> 16) & 0x0000FFFF)
#define PMAN_TBL_GET_DINDEX(entry) ((entry & 0x0000FFFF) >> 6)

/* Task not present record format */
#define PA_TASKTBL_SWAPPED_FLAG 0x2
#define PA_TASKTBL_SWAPADDR(x)	((unsigned int)x & ~0xFFF)
#define PA_TASKTBL_SWAPPED(x)	(((unsigned int)x & PA_TASKTBL_SWAPPED_FLAG) && !PG_PRESENT(x))

/* Point modifiers on the stealing thread for page candidate selection. */
#define PA_POINTS_TBL_MODIFIER		2	// page is a page table
#define PA_POINTS_TBL_SWP_MODIFIER	2	// page is a page table with records swapped (requires IO)
#define PA_POINTS_DIRTY_MODIFIER	4	// page is dirty (requires IO)
#define PA_POINTS_A_MODIFIER		2	// A bit is turned on.
#define PA_POINTS_AGE_MODIFIER		2	// This value will be multiplied by the page age.

/* Max stealing thread iterations until it starts stealing page tables from a region. */
#define PA_TBL_ITERATIONS	32
/* Max stealing thread iterations until it decides to end freeing space on it current region. */
#define PA_MAX_ITERATIONS	64


#endif


