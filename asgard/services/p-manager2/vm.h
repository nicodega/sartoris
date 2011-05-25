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

#ifndef PMANVMH
#define PMANVMH

#include "mem_layout.h"
#include "types.h"
#include "taken.h"
#include "io.h"
#include "page_stack.h"
#include "formats/ia32paging.h"
#include "rb.h"
#include "vmm_memarea.h"

#define VMM_INITIAL_AGE 4

/* how many megabytes we are feeding into the pool, must be a multiple of 4 */
#define POOL_MEGABYTES 				vmm.pool_MB
/* How many bytes of the pool are being used for page tables */
#define POOL_TABLES_SIZE			(vmm.vmm_tables * 0x1000)
/* first actual page, since we are using the first few */
/* as page tables to access the page pool itself       */
#define FIRST_PAGE(x) 	(((unsigned int)x) + POOL_TABLES_SIZE)
/* This will map a pool physical address to the linear pman address */
#define PHYSICAL2LINEAR(x) ( (((unsigned int)x) - PMAN_POOL_PHYS) + PMAN_POOL_LINEAR )
/* This will map a pool linear address to the physical address */
#define LINEAR2PHYSICAL(x) ( (((unsigned int)x) - PMAN_POOL_LINEAR) + PMAN_POOL_PHYS )
/* Translate a task linear address to sartoris address casting to the specified type. */
#define TRANSLATE_ADDR(addr, type) ((type)((UINT32)addr + SARTORIS_PROCBASE_LINEAR))

/* 
Structure used on process Table/Dir 
for PG assigned records. 
*/
struct vmm_not_present_record
{
	UINT32 addr:20;		// Swap file address (if swapped)
	UINT32 write:1;		// 1 if write permission is granted
	UINT32 exec:1;		// 1 if execute permission is granted
	UINT32 unused:8;	// reserved, set to 0
	UINT32 swapped:1;	// If 1 page was sent to swap file
	UINT32 present:1;	// If 1 record is NOT valid (i.e. page is present)
} PACKED_ATT;

/*
A page entry will contain a page address or an 
information record
*/
struct vmm_page_entry
{
	union
	{
		UINT32 phy_page_addr;					// A physical address for a page
		struct vmm_page_tbl_entry ia32entry;	// A Page table entry (IA32)
		struct vmm_not_present_record record;	// A Not present record with swap information
	} entry;
} PACKED_ATT;

/* Page Table */
struct vmm_page_table
{
	struct vmm_page_entry pages[1024];
} PACKED_ATT;

/* Page Directory */
struct vmm_page_directory
{
	union
	{
		struct vmm_not_present_record record;
		struct vmm_dir_entry ia32entry;			// IA 32 directory entry
		UINT32 b;
	} tables[1024];
} PACKED_ATT;

/*
Assigned record. This will be placed on PMAN table entry. 

	31   ...  12 | 11  ...   6 | 5 4 3 2 1 | 0
		 Task    |  dir index  |   Flags   | N
*/

struct vmm_pman_assigned_record
{
	UINT32 not_assigned:1;	// If 0 page is assigned.
	UINT32 flags:5;			// Flags.	
	UINT32 dir_index:6;		// Directory index for the page on the task.
	UINT32 task_id:16;		// Owner task.	
} PACKED_ATT;

/* A pman Pool Page table. */
struct vmm_pman_assigned_table
{
	struct vmm_pman_assigned_record entries[1024];
} PACKED_ATT;

/* Pman Pool Page Directory. */
struct vmm_pman_assigned_directory
{
	struct vmm_pman_assigned_table *table[1024];  // Assigned table linear (PMAN) address.
} PACKED_ATT;

/* Memory regions for tasks. */
/*
This structures will be kept on a tree in the task struct
*/
#define VMM_MEMREG_IDNODE2REG(a) ((struct vmm_memory_region*)((UINT32)a - ((sizeof(struct vmm_memory_region) - sizeof(rbnode)))))
// This define returns a pointer to a memory regions, based on it's tsk_node.
#define VMM_MEMREG_MEMA2MEMR(a) ((struct vmm_memory_region*)((UINT32)VMM_MEMREG_IDNODE2REG(a) + sizeof(ma_node)))

struct vmm_memory_region
{
    UINT16 owner_task;      
	UINT16 type;            // Memory region type: SHARED, FMAP, MMAP
	UINT16 flags;           // Exclusive, write, execute, etc
	ADDR descriptor;        // The typed global memory region descriptor to which this memory regions belongs.
                            // NOTE: Many memory regions might contain the same descriptor.
	ma_node tsk_node;       // Node on task memory areas
    rbnode tsk_id_node;     // Node on task memory regions by id tree
    struct vmm_memory_region *next; // on the descriptor "regions" list
    struct vmm_memory_region *prev; // on the descriptor "regions" list
} PACKED_ATT;

/* Memory region Types */
#define VMM_MEMREGION_SHARED   1		// This region is shared with other task
#define VMM_MEMREGION_FMAP     2		// This region is File Mapped
#define VMM_MEMREGION_MMAP     3		// This region is Mapped to Physical Memory
#define VMM_MEMREGION_LIB      4		// This region is Mapped to a shared library

/* Memory region Flags */
#define VMM_MEM_REGION_FLAG_NONE		0
#define VMM_MEM_REGION_FLAG_EXCLUSIVE	1
#define VMM_MEM_REGION_FLAG_WRITE		2
#define VMM_MEM_REGION_FLAG_EXECUTE		3

/*
Generic descriptor
*/
struct vmm_descriptor
{					
	struct vmm_memory_region *regions;  // memory regions referencing this descriptor
};

/* 
File Mapping Regions descriptor. 
*/

#define MEMREGNODE2FMAPDESC(a) ((struct vmm_fmap_descriptor*)(VMM_MEMREG_IDNODE2REG(a)->descriptor))
#define GNODE2FMAPDESC(a) ((struct vmm_fmap_descriptor*)((unsigned int)a - sizeof(struct vmm_fmap_descriptor) + sizeof(rbnode)))

struct vmm_fmap_descriptor
{					
	struct vmm_memory_region *regions;  // memory regions referencing this descriptor
    UINT16 references;
    
	UINT32 release_addr;				// Helper variable used on release
	UINT32 offset;						// file offset
	struct fsio_event_source iosrc;		// IO source for this descriptor
	struct io_event io_finished;		// This event will be used when opening or closing the map.
	UINT16 status;
	UINT16 padding;
	UINT16 creating_task;
    rbnode gnode;                       // node on the global fmaps descriptor tree
} PACKED_ATT;

/* FMap descriptor status */
#define VMM_FMAP_ACTIVE            1
#define VMM_FMAP_CLOSING           2
#define VMM_FMAP_FLUSHING          3
#define VMM_FMAP_CLOSING_RELEASE   4

/* 
Shared Pages Region Descriptor 
*/

#define MAREA2SHAREDDESC(a) ((struct vmm_shared_descriptor*)((unsigned int)a - sizeof(struct vmm_shared_descriptor) + sizeof(ma_node)))

struct vmm_shared_descriptor
{
	struct vmm_memory_region *regions;      // memory regions referencing this descriptor
    UINT16 id;
    UINT16 owner_task;					    // original owner of the page
    struct vmm_memory_region *owner_region; // region representing this shared area on the owner task
} PACKED_ATT;

/* 
Physical to linear mapping. (a task might require to map a given physical address to it's logical address) 
*/

#define MAREA2PHYMDESC(a) ((struct vmm_phymap_descriptor*)((unsigned int)a - sizeof(struct vmm_phymap_descriptor) + sizeof(ma_node)))

struct vmm_phymap_descriptor
{
	struct vmm_memory_region *regions;  // memory regions referencing this descriptor
    int exclusive;                      // 1 if region is assiged exclusively.
    int references;
    ma_node area;                       // physical memory area on vmm phy_mem_areas
} PACKED_ATT;

/* Shared library descriptor */
struct vmm_slib_descriptor
{
	struct vmm_memory_region *regions;  // memory regions referencing this descriptor
    int references;                     // references to the library
    char *path;                         // library path
    struct vmm_slib_descriptor *next; // used on the libraries list on vmm main struct
    struct vmm_slib_descriptor *prev;
    int task;                           // the task for this shared library (where we will get our pages from)
    BOOL loaded;                        
} PACKED_ATT;

struct thread_vmm_info;

/* Task VMM structure. */
struct task_vmm_info
{
	struct vmm_page_directory *page_directory;	// vmm directory table for the process.
	ADDR swap_free_addr;			/* If NOT NULL it contains swap address assigned for an IO operation */
	UINT32 page_count;				/* Pages granted so far to this task                                 */
	UINT32 swap_page_count;			/* Pages on swap for this task                                       */
	UINT32 expected_working_set;	/* How much do we expect this task to grow in the future             */
	UINT32 max_addr;				/* Task max address granted (brk)                                    */
	INT32 swap_read_smo;			/* Used for reading a page from swap partition.                      */
	UINT32 table_swap_addr;			/* Used when freing swap pages for the process. Holds swap address 
                                    for the last page table read from Swap Partition.                    */

	rbt regions_id;		            /* Task memory regions by id                                         */
    memareas regions;               /* Task memory regions by linear address (fmaped or phy mapped)      */
    rbt wait_root;                  /* This is the root of a red black tree with threads (not 
                                               necessarily threads from this task) waiting for pages.    */
    rbt tbl_wait_root;              /* Red Black tree root of threads waiting for page tables.
                                               NOTE: If two threads are waiting for the same page, only 
                                               one of them will be on this tree.                         */
} PACKED_ATT;

struct thread_vmm_info
{
	ADDR fault_address;					    /* Linear address for last page fault						  */
	ADDR page_in_address;				    /* Page granted for last page fault							  */
	struct vmm_not_present_record fault_entry;	/* ??                                                     */
	UINT32 read_size;				        /* Size of data being read from disk for the last page fault  */
	UINT32 page_displacement;				/* Write possition on the page of the last page fault         */
	UINT32 page_perms;						/* Page permissions                                           */
	UINT32 fault_smo;						/* Used By Swap IO subsystem for the SMO.                     */
	struct vmm_memory_region *fault_region;	/* If page fault was issued on a memory region, this will 
											   contain region structure.                                  */
	struct fsio_event_source fmap_iosrc;	/* IO Event source Descriptor for FMAP                        */

    int fault_task;                         /* The task for which the page fault was generated. It might 
                                               not be the same task as the thread owner.                  */
    /* Node information on the task red-black tree */
    rbnode pg_node;                         /* Node on the pages red black tree                           */
    rbnode tbl_node;                        /* Node on the swap table red black tree                      */
} PACKED_ATT;

/* Virtual Memory Manager (VMM) Main Structure */
struct vmm_main
{
	struct vmm_pman_assigned_directory assigned_dir;

	struct multiboot_info *multiboot;
	/* 
	Low memory stack. 
	Pages below the 16MB mark will be kept on this stack.
	*/
	struct page_stack low_pstack;
	/* 
	Page stack (above 16 MB) 
	*/
	struct page_stack pstack;
	/*
	Taken structure 
	*/
	struct taken_directory taken;
	/*
	vmm start pointer 
	*/
	void *vmm_start;
	UINT32 vmm_size;        // pool size
	UINT32 vmm_tables;      // pool size in tables

	UINT32 pysical_mem;     // physical memory on the system (max address)
	UINT32 pool_MB;         // pool memory in megabytes
	
	/*
	This values will be used for page Swapping
	*/
	UINT32 reserved_mem;    // memory reserved for PMAN dynamic allocator.  
	UINT32 pman_mem;        // process manager assigned memory
	UINT32 available_mem;	// pool available memory
	UINT32 max_mem;		    // System has enough memory and does not need more
	UINT32 mid_mem;         // System has enough memory but lets keep on strealing a few pages
	UINT32 min_mem;         // If reached this value we are starving for memory

	UINT32 swap_size;	        // total swap size in pages
	UINT32 swap_available;	    // total swap available pages
	UINT32 swap_ldevid;	        // logic device id for swap device.
	BOOL   swap_present;		// Whether Swap is present or not.
	UINT32 swap_start_index;
	UINT32 swap_end_index;
	UINT32 swap_thr_distance;

	/* Memory regions */
    rbt      fmap_descriptors;    // each file will have a file id. This tree will contain all files taken by PMAN.
    memareas phy_mem_areas;       // a structure with physical memory areas
                                  // for telling if an area is already phy mapped.

    /* libraries */
    struct vmm_slib_descriptor *slibs;
} vmm;

void vmm_init_thread_info(struct pm_thread *thread);

/* Initialize vmm */
INT32 vmm_init(struct multiboot_info *multiboot, UINT32 ignore_start, UINT32 ignore_end);
/* Add a Memory block to the vmm managed memory space */
INT32 vmm_add_mem(struct multiboot_info *multiboot, UINT32 start, UINT32 end);

/* Internal PMAN paging functions */

// Get a page for process manager internal use
ADDR vmm_pm_get_page(BOOL lowmem);
// Return a page to the free set
void vmm_pm_put_page(ADDR page_addr);

// initialize Task VMM information structure
void vmm_init_task_info(struct task_vmm_info *vmm_info);
// Decide if a task can be loaded, based on the expected working set and physical/virtual memory available
BOOL vmm_can_load(struct pm_task *task);

/* Process Related PMAN paging functions */

// Get taken entry for this (PMAN) linear address
struct taken_entry *vmm_taken_get(ADDR laddress);
// Get a free page for a process page directory
ADDR vmm_get_dirpage(UINT16 task_id);	
// Get a free page for a process page table
ADDR vmm_get_tblpage(UINT16 task_id, UINT32 proc_laddres);
// Get a free page for a process 
ADDR vmm_get_page(UINT16 task_id, UINT32 proc_laddres);
// Get a free page for a process specifying if it must be taken from low memory
ADDR vmm_get_page_ex(UINT16 task_id, UINT32 proc_laddress, BOOL lowmem);
// Return a process page to the pool
void vmm_put_page(ADDR address);
// map a page to a process linear address
void vmm_map(ADDR page_addr, ADDR linear);
// claim a task address space completely
void vmm_claim(UINT16 task);
// Interrupt handler for PF
void vmm_paging_interrupt_handler();
// Close task address space (free descriptors, flush FMAPs, return pages to pool)
void vmm_close_task(struct pm_task *task);
// Unmap a page from pman linear space. Used for common pages (not directories nor tables).
void vmm_unmap_page(UINT16 task_id, UINT32 proc_laddress);
// Page in, setting initial age
INT32 pm_page_in(UINT16 task_id, ADDR linear, ADDR physical, UINT32 level, INT32 attrib);
/* Swap */

BOOL vmm_check_swap_tbl(struct pm_task *task, struct pm_thread *thread, ADDR pg_laddr);
BOOL vmm_check_swap(struct pm_task *task, struct pm_thread *thread, ADDR pg_laddr);
void vmm_swap_empty(struct pm_task *task, BOOL iocall);
UINT32 swap_free_table_callback(struct pm_task *task, UINT32 ioret);
UINT32 swap_page_read_callback(struct pm_thread *thread, UINT32 ioret);
UINT32 swap_table_read_callback(struct pm_thread *thread, UINT32 ioret);
// Begin swap initialization
void vmm_swap_init();
// Invoked when a swap partition is found on the main ata controler
void vmm_swap_found(UINT32 partition_size, UINT32 ldevid);
// Get table physical address for the (process) linear address
struct vmm_page_table *vmm_get_tbl_physical(UINT16 task, ADDR laddress);
// Get physical address for the (process) linear address
ADDR vmm_get_physical(UINT16 task, ADDR laddress);
// Get assigned record for the (process) linear address
struct vmm_pman_assigned_record *vmm_get_assigned(UINT16 task_id, UINT32 proc_laddress);
// Set flags on the page taken entry
void vmm_set_flags(UINT16 task, ADDR laddress, BOOL eflag, UINT32 flag, BOOL enabled);
// finish a task with an exception, and wake al threads waiting 
// for the same page/swapped table, when an ioerror occurrs while fetching 
// a page through IO
void vmm_page_ioerror(struct pm_thread *thread, BOOL removeTBLTree);
// This function will remove all threads killed and destroy it's task if it was also killed.
void vmm_check_threads_pg(struct pm_thread **thr, BOOL removeTBLTree);
// e-enable Threads Waiting for the same page.
void vmm_wake_pf_threads(struct pm_thread *thread);
// Page fault handler
// thr_id: If not NULL it will be set to the page fault rising thread id. (If its an internal call this MUST contain the thread id)
// internal: If TRUE, handler was invoked by pman, FALSE if invoked by PF interrupt.
BOOL vmm_handle_page_fault(UINT16 *thr_id, BOOL internal);
// Callback function invoked when finished seeking on an executable file
INT32 vmm_elffile_seekend_callback(struct fsio_event_source *iosrc, INT32 ioret);
// Callback function invoked when finished reading from an executable file
INT32 vmm_elffile_readend_callback(struct fsio_event_source *iosrc, INT32 ioret);

// Aging thread ep
void vmm_page_aging();
// Stealing thread ep
void vmm_page_stealer();
// Calculate a page points (the bigger the return value, the less the chance page will be paged out)
UINT32 calculate_points(struct taken_entry *entry, UINT32 pm_dir_index, UINT32 pm_tbl_index, BOOL ioavailable, BOOL *iorequired, UINT16 *task_id, UINT32 *task_laddress);
// Callback invoked when a page has been succesfully written to disk
UINT32 vmm_page_swapped_callback(struct io_slot *ioslot, UINT32 ioret);

/* Swap Bitmap */
void init_swap_bitmap();
void swap_free_addr(UINT32 swap_addr);
void swap_use_addr(UINT32 swap_addr, UINT32 perms);
UINT32 swap_get_addr();
UINT32 swap_get_perms(struct pm_task *task, ADDR proc_laddress);

/* Memory regions globlal list handling */
struct vmm_region_descriptor *vmm_regiondesc_get(UINT16 id, UINT16 type);
void vmm_regiondesc_add(ADDR prdesc);
void vmm_regiondesc_remove(ADDR prdesc);
void vmm_regiondesc_remove_byid(UINT16 ID);
UINT16 vmm_regiondesc_getid();
UINT32 vmm_region_pageperms(struct vmm_memory_region *mreg);
struct vmm_memory_region *vmm_region_get_bydesc(struct pm_task *task, struct vmm_descriptor *desc);
struct vmm_memory_region *vmm_region_get(struct pm_task *task, UINT32 tsk_lstart);
void vmm_region_add(struct pm_task *task, struct vmm_memory_region *mreg);
void vmm_region_remove(struct pm_task *task, struct vmm_memory_region *mreg);

/* FMAP */
BOOL vmm_fmap_flush(struct pm_task *task, ADDR tsk_lstart);
BOOL vmm_page_filemapped(struct pm_task *task, struct pm_thread *thread, ADDR page_laddr, struct vmm_memory_region *mreg);
UINT32 vmm_fmap(UINT16 task_id, UINT32 fileid, UINT16 fs_task, ADDR start, UINT32 size, UINT32 perms, UINT32 offset);
BOOL vmm_fmap_release(struct pm_task *task, ADDR tsk_lstart);
void vmm_fmap_close_all(struct pm_task *task);

/* PHY MAP */
BOOL vmm_phy_mmap(struct pm_task *task, ADDR py_start, ADDR py_end, ADDR lstart, ADDR lend, BOOL exclusive);
void vmm_phy_umap(struct pm_task *task, ADDR lstart);

/* SHARED PAGES */
BOOL vmm_page_shared(struct pm_task *task, ADDR proc_laddr, struct vmm_memory_region *mreg);
void vmm_share_remove(struct pm_task *task, UINT32 lstart);
BOOL vmm_share_create(struct pm_task *task, ADDR laddr, UINT32 length, UINT16 perms);
BOOL vmm_share_map(UINT32 descriptor_id, struct pm_task *task, ADDR laddr, UINT32 length, UINT16 perms);

/* SHARED LIBRARIES */
int vmm_page_lib(struct pm_task *task, struct pm_thread *thread, ADDR proc_laddr, struct vmm_memory_region *mreg);
int vmm_lib_load(struct pm_task *task, char *path, int plength, UINT32 vlow, UINT32 vhigh);
BOOL vmm_lib_loaded(struct pm_task *libtask, BOOL ok);
BOOL vmm_lib_unload(struct pm_task *task, struct vmm_memory_region *mreg);
ADDR vmm_lib_get(char *path);

/******************************************************************************************

Minimal metadata system adaptation for page swapping and memory mappings.

The main idea is to use only 32 bits per page.

METADATA:

In order to improve performance, and avoiding page_in/page_out for page tables when checking if a page is swapped, 
what we will do is maintain another set of tables, with an interger per physical memory page. (hence no more than 4MB will be used on a 4GB system). 
We will call this structure the taken tables. This structure will be used this way:

If a physical slot has been granted to a task, record will be set like this:

- If page is a page directory, the page will remain mapped on pman memory, and it's taken table entry will contain a directory record.

- If page is a page table, the page will remain mapped on pman memory, and it's taken table entry will contain a tbl record.

- If it's a level 2 page, the page will be un-mapped from pman memory, and a lvl 2 page record will be created on the taken structure.

Taken structure entry format:


		31 ........ 3 | 2   1    | T
			info      | TBL DIR  | 0

	- If T is 1 the record is valid. Else page is not taken.

	- If DIR is 1 this record has been assigned to a page directory. Info will contain the task id.

	- if TBL is 1 this record has been asigned to a page table. Info will have the folowing format:
		
			31 ........16 | 15 ............ 6 | 5  4   3
			     task     |   index on dir    |PF SRV IOLOCK  

	PF eflag on page table taken entries will be used to synchronise the page fault handling 
	function and the page stealing thread.
	Page stealer will page out the table/page while it's been sent to IO, and send_msg will be performed under cli.
	When ATAC response arrives PF flag will be checked. If it's set to 1 a page fault raised on that
	page table, and it's swap procedure must be interrupted. If 0 page table can be safely removed.
	

	- If TBL is 0 and DIR is 0 this address has been assigned to a page. Info field will have the following
	  format:

			31 ........16 | 15 ............ 6  | 5   4   3       
			    index     |       flags        | U  SRV IOLOCK  

	
		Flags field will have the following format:

		  15 14 13 12 11  10   9   8  7   6
		  U  U  U  U  U   U   PMAN F  S  LCK

		- If LCK is 1 this record has been locked.

		- If F is 1 this page is mapped to a file. index on table contains an index onto
		an internal pman list of file mapped pages.

		If S is 1 this page is shared. index on table contains an index onto
		an internal pman list of shared pages. 

	    Also the process manager table entry will be marked as not present and the record will indicate the task/table index 
	    owning the page.

		    31   ...  12 | 11  ...   6 | 5 4 3 2 1 | 0
		         Task    |  dir index  |   Flags   | N

        N = 1 then Null record (page is present)

When a page table/ lvl 2 page is assigned to a task, its entry on the task's page dir/table will have the following format:

	    31 ........ 12 | 11 ..  1  | 0
          file index   |       SWP | 0

If SWP is 1 the page is on swap file and file index is it's file offset. File offset is in 4kb multiples.

*******************************************************************************************/


#endif

