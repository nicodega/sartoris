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


#ifndef __PMAN_INTERNALS_HEADER
#define __PMAN_INTERNALS_HEADER

#include <sartoris/kernel.h>
#include <services/pmanager/services.h>
#include <drivers/screen/screen.h>
#include <os/layout.h>
#include "initfs.h"
#include "elf.h"
#include "pman_malloc.h"
#include "signals.h"
#include "page_alloc.h"

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

/* This define will enable an experimental mode for treating paging 
interrupts when interrupting a current executing int. */

#define NAME_LENGTH 20

// PMAN_COMMAND_PORT is 4
#define FILEOPEN_PORT 5
#define FILESEEK_PORT 6
#define FILEREAD_PORT 7
#define FILECLOSE_PORT 8
#define ELF_RESPONSE_PORT 9
// PMAN_SIGNALS_PORT	10		// defined on include/services/pmanager/signals.h
// PMAN_EVENTS_PORT		11		// defined on include/services/pmanager/signals.h
#define INITFS_PORT		FILEOPEN_PORT	// port used for ofs initialization (yeap, it overlaps with FILEOPEN_PORT)
#define INITPA_PORT		INITFS_PORT
#define SWAP_READ_PORT		12
#define SWAP_WRITE_PORT		13
#define SWAP_DESTROY_READ_PORT		14

#define MAX_PATH_LENGTH 32

struct pm_task {
  int  state;                          /* one of TSK_NOTHING, TSK_LOADING, TSK_NORMAL, TSK_KILLED    */
  int  flags;
  int  first_thread;                   /* id of first thread                                         */
  int  num_threads;                    /* thread count                                               */
  int  fs_task_id;                     /* task id of file filesystem's host task                     */
  int  open_fd;                        /* open file descriptor for this task, zero if none           */
  int  file_size;                      /* size of the program file, used for paging of raw binary    */
  char full_path[MAX_PATH_LENGTH];     /* path to file associated with task                          */
  int  creator_task_id;                /* id of the task that originated the request                 */
  short response_port;                 /* port for diagnostics                                       */
  short req_id;                        /* internal id for diagnostics (set by creator task)          */
  void *page_dir;
  int ret_value;						/* used when closing the file, for finished msg 	     */
  int destroy_sender_id;
  short destroy_req_id;
  short destroy_ret_port;  
  struct Elf32_Ehdr elf_header;			/* The elf header of the process */
  char *elf_pheaders;					/* elf program headers */
  int task_smo_id;
  unsigned int swap_free_addr;
  unsigned int page_count;				/* Pages granted so far to this task */
  unsigned int swap_page_count;			/* Pages on swap for thiss task */
} PACKED_ATT;

struct signal_container;

struct pm_thread {
  int task_id;
  int state;							/* one of THR_NOTHING, ... THD_KILLED                         */
  int flags;
  void *fault_address;
  void *page_in_address;
  int fault_smo_id;
  int fault_next_thread;				/* Used for keeping a list of threads waiting for the same page */
  int swaptbl_next;						/* Used for keeping a list of threads waiting for the same page table (but different page) */
  unsigned int fault_entry;
  unsigned int read_size;
  int page_displacement;
  int page_perms;
  int next_thread;						/* next thread in task's thread linked list                   */
  int interrupt;
  void *stack_addr;						/* Thread stack address. */
} PACKED_ATT;

/* Thread status */
#define THR_NOTHING  0    /* the thread is not being used       */
#define THR_WAITING  1    /* the thread actually is a thread    */
#define THR_RUNNING  2    /* the thread is running as of now    */
#define THR_BLOCKED  3    /* the thread is blocked              */
#define THR_INTHNDL  4    /* the thread is an interrupt handler */
#define THR_KILLED   5    /* the thread is on death row         */
#define THR_EXEPTION 6	  /* the thread was stopped because of an exception */
#define THR_SBLOCKED 7    /* the thread is blocked (by a signal)*/
#define THR_INTERNAL 8	  /* this is an internal thread         */

/* Task status */
#define TSK_NOTHING  0    /* the task is not being used         */
#define TSK_LOADING  1    /* the task is being loaded           */
#define TSK_NORMAL   8
#define TSK_KILLED   16   /* the task is on death row           */

/* Thread flags */
#define THR_FLAG_PAGEFAULT		1	/* page fault is being processed							*/
#define THR_FLAG_PAGEFAULT_TBL  2	/* Page fault being attended also raised a page table fault */

/* Task flags */
#define TSK_FLAG_FILEOPEN		4	/* task file is open on the filesystem		*/
#define TSK_FLAG_ELF_HDR		8   /* elf header is being retrived				*/
#define TSK_FLAG_ELF_PHDR		16  /* elf program headers are being retrieved	*/
#define TSK_FLAG_SERVICE		32  /* this task is a service					*/
#define TSK_FLAG_SYS_SERVICE	(TSK_FLAG_SERVICE | 64)  /* this task is a system service			*/


/* Task and thread ids used during initialization, and
created by services without asking pman are below this numbers */
#define INIT_SRVTHR		20		  
#define INIT_SRVTASK	10		  

/* Thread and task id assignation boundaries */
#define NUM_SERV_THR (MAX_THR - INIT_SRVTHR) //48
#define NUM_USER_THR (MAX_THR - INIT_SRVTHR) //(MAX_THR - NUM_SERV_THR)

#define NUM_SERV_TSK (MAX_TSK - INIT_SRVTASK) // 16
#define NUM_USER_TSK (MAX_TSK - INIT_SRVTASK) //(MAX_TSK - NUM_SERV_TSK)

#define BASE_USER_TSK INIT_SRVTASK	//NUM_SERV_TSK
#define BASE_USER_THR INIT_SRVTHR	//NUM_SERV_THR

#define FIRST_SERV_TSK  INIT_SRVTASK // 0
#define LAST_SERV_TSK   (NUM_SERV_TSK - 1)
#define FIRST_USER_TSK  INIT_SRVTASK //(NUM_SERV_TSK)
#define LAST_USER_TSK   (NUM_USER_TSK - 1) //(NUM_SERV_TSK + NUM_USER_TSK - 1)

#define FIRST_SERV_THR  INIT_SRVTHR			// 0
#define LAST_SERV_THR   (NUM_SERV_THR - 1)
#define FIRST_USER_THR  INIT_SRVTHR			// (NUM_SERV_THR)
#define LAST_USER_THR   (NUM_USER_THR - 1)	// (NUM_SERV_THR + NUM_USER_THR - 1)

/* pmanager.c */
int is_int0_active();
void ret_from_sched();

/* commands.c */
void pm_process_command(struct pm_msg_generic *msg, int task_id);
void pm_create_task(struct pm_msg_create_task *msg, int task_id);
int  pm_destroy_task(int task_id);
void pm_create_thread(struct pm_msg_create_thread *msg, int creator_task_id );
int  pm_destroy_thread(int thread_id);
void pm_inform_result(struct pm_msg_generic *msg, int task_id, int status, int new_id, int new_id_aux);
int  pm_get_free_task_id(int lower_bound, int upper_bound);

/* fileio.c */
void pm_request_file_open(int new_task_id, struct pm_msg_create_task *msg);
void pm_request_seek_end(int new_task_id, int file_id);
void pm_request_tell(int new_task_id, int file_id);
void pm_request_seek(int thread_id, unsigned int offset);
void pm_request_read(int thread_id, void *pg_addr, unsigned int size);
void pm_request_close(int task_id);
void pm_process_file_opens(void);
void pm_process_seek_ends(void);
void pm_process_tells(void);
void pm_process_seeks(void);
void pm_process_reads(void);
void pm_process_closes(void);

/* exceptions.c */
void pm_spawn_handlers(void);
void pm_handler(void);

/* paging.c */
void pm_init_page_pool(void *, void *);
void pm_claim_address_space(void *page_dir);
int pm_handle_page_fault(int *thr_id, int internal_fault);
void *pm_get_page();
void pm_assign(void *pgaddr_phys, unsigned int pman_tbl_entry, struct taken_entry *tentry);
void set_taken(struct taken_entry *tentry, void *phys_addr);
struct taken_entry *get_taken(void *phys_addr);
void pm_put_page(void *physical);
void wake_pf_threads(int thread_id);
int pm_page_in(int task, void *linear, void *physical, int level, int attrib);

/* Init Helpers */
unsigned int create_service(int task, int thread, int invoke_level, char *image_name, unsigned int forced_size, unsigned int forced_physical, int load_all);
int get_img_header(struct service_header *header, char *img_name);
int pminit_elf_open(char *imgname);
int pminit_elf_seek(int task_id, unsigned int offset);
int pminit_elf_read(int task_id, int size, char *buffer);
int strcmp(char *c1, char *c2);
void pm_init_tasks(void);
void pm_init_threads(void);
unsigned int put_pages(int task, int use_fsize, void *phys_start);

/* Init */
void pman_init_stage2();
void process_manager(void);

/* init fs */
void fsinit_begin();
void fsinit_process_msg();

/* System Services */
unsigned int pm_spawn_services();

#define SARTORIS_PROCBASE_LINEAR	0x800000					// base linear address used by sartoris

/* Task defines */
#define PM_TASK_SIZE				(0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR)    // 4GB less base
#define PM_THREAD_STACK_BASE		0x7FFFFFFF   				// 2GB 
#define PM_MAPPING_BASE				0x80000000					// 2GB this is where we will start mapping libs or other stuff

/* Memory regions size */
#define PMAN_CODE_SIZE				0x200000					// 2 MB for pman code
#define PMAN_INITIMG_SIZE			0x100000					// size of init images region 
#define PMAN_MULTIBOOT_SIZE			0x10000						// size of the multiboot region
#define PMAN_MALLOC_SIZE			((0x400000 - PMAN_MULTIBOOT_SIZE) - PMAN_CODE_SIZE)

/* Physical Memory Layout */
#define PMAN_PHYS_BASE				0x100000							// where sartoris and non available memory ends
#define PMAN_PDIR_PHYS				PMAN_PHYS_BASE						// PMAN page directory location
#define PMAN_PTBL_PHYS				(PMAN_PDIR_PHYS + 0x1000)			// PMAN first page table location
#define PMAN_CODE_PHYS				(PMAN_PTBL_PHYS + 0x1000)			// PMAN code start
#define PMAN_MULTIBOOT_PHYS			(PMAN_CODE_PHYS + PMAN_CODE_SIZE)	// physical location of multiboot information
#define PMAN_MALLOC_PHYS			(PMAN_MULTIBOOT_PHYS + PMAN_MULTIBOOT_SIZE);
#define PMAN_POOL_PHYS				(0x400000 + PMAN_CODE_PHYS)	// the pool will begin at the end of the first PMAN 4MB
#define PMAN_INIT_RELOC_PHYS		(PMAN_POOL_PHYS + 0x400000)	// where we will relocate the init images 
																// (if this is changed stage 2 copy procedure must be changed too)
#define PMAN_LOWMEM_PHYS			(PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE)	// where we can begin assigning memory, under 16mb
#define PMAN_HIGH_MEM_PHYS			0x1000000					// 16 MB and up

#define PMAN_SARTORIS_MMAP_PHYS		0x100000	// where bootinfo is left by sartoris on physical memory
#define PMAN_SARTORIS_MMAP_LINEAR	0x3F0000	// where sartoris maps bootinfo on init task
#define PMAN_SARTORIS_INIT_PHYS		0x800000	// where sartoris left the init image

/* Pman linear memory layout */
#define PMAN_BASE_LINEAR			0x0							// where pman code begins (from pman point of view)
#define PMAN_MULTIBOOT_LINEAR		(PMAN_BASE_LINEAR + PMAN_CODE_SIZE)				// multiboot linear position
#define PMAN_MALLOC_LINEAR			(PMAN_MULTIBOOT_LINEAR + PMAN_MULTIBOOT_SIZE)	// where malloc buffer can start
#define PMAN_POOL_LINEAR			(PMAN_MALLOC_LINEAR + PMAN_MALLOC_SIZE)	// this will be exactly at 0x400000			

#define PMAN_STAGE1_MAPZONE			0x300000	// were we will map on stage 1 for copy (linear)

#define PMAN_STAGE2_MAPZONE_SOURCE	0x201000	// were we will map on stage 2 for copy (linear)
#define PMAN_STAGE2_MAPZONE_DEST	0x202000	// were we will map on stage 2 for copy (linear)

/* Pman defines */
#define PMAN_CODE_PAGES				(PMAN_CODE_SIZE / 0x1000)
#define PMAN_MULTIBOOT_PAGES		(PMAN_MULTIBOOT_SIZE / 0x1000)
#define PMAN_MALLOC_PAGES			(PMAN_MALLOC_SIZE / 0x1000)

#define PMAN_STACK_ADDR				(PMAN_CODE_SIZE - 0x6000)
#define PMAN_EXSTACK_ADDR			(PMAN_CODE_SIZE)
#define PMAN_AGING_STACK_ADDR		(PMAN_CODE_SIZE - 0x2000)	
#define PMAN_STEALING_STACK_ADDR	(PMAN_CODE_SIZE - 0x4000)

/* Pool defines */

/* how many megabytes we are feeding into the pool, must be a multiple of 4 */
#define POOL_MEGABYTES 				pool_megabytes 
/* How many bytes of the pool are being used for page tables */
#define POOL_TABLES_SIZE			(POOL_MEGABYTES/4 * 0x1000)
/* first actual page, since we are using the first few */
/* as page tables to access the page pool itself       */
#define FIRST_PAGE(x) 	(((unsigned int)x) + POOL_TABLES_SIZE)
/* This will map a pool physical address to the linear pman address */
#define PHYSICAL2LINEAR(x) ( (((unsigned int)x) - PMAN_POOL_PHYS) + PMAN_POOL_LINEAR )
/* This will map a pool linear address to the physical address */
#define LINEAR2PHYSICAL(x) ( (((unsigned int)x) - PMAN_POOL_LINEAR) + PMAN_POOL_PHYS )

/* Paging useful defines */
#define PM_LINEAR_TO_DIR(linear)	((unsigned int)linear >> 22)
#define PM_LINEAR_TO_TAB(linear)	(((unsigned int)linear & 0x003FFFFF) >> 12)
#define PG_ADDRESS(physical)		((unsigned int)physical & 0xFFFFF000)
#define PG_PRESENT(entry)			((unsigned int)entry & 0x00000001)
#define PM_LINEAR_TAB_PAD(linear)	(((unsigned int)linear & 0x003FFFFF))

#define PG_A_BIT 0x20
#define PG_D_BIT 0x40

/* Macro to get the process manager table containing the given pman linear address */
#define PM_TBL(linear)				(PMAN_POOL_LINEAR + PM_LINEAR_TO_DIR((unsigned int)linear + SARTORIS_PROCBASE_LINEAR) * 0x1000)

/* Init pool dependant defines */
#define PMAN_INITSTAGE2_LINEAR		PHYSICAL2LINEAR(PMAN_INIT_RELOC_PHYS)	// init will be paged in when paging the pool


#define STOP 						for(;;)
#define STACK_ADDR(a) 				((unsigned int)a - 4)

/* Memory size variables */
extern unsigned int physical_memory;
extern unsigned int pool_megabytes;

/* default value if no mem info is passed at boot time */
#define PMAN_DEFAULT_MEM_SIZE 0x2000000 
/* System memory define */
#define PMAN_MEM_SIZE physical_memory

/* Elf functions */
int pm_request_elf_seek(int task_id, unsigned int offset);
int pm_request_elf_read(int task_id, int size, char *buffer);
void pm_process_elf_res();

int elf_begin(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*));
int elf_seekphdrs(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*));
int elf_readphdrs(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*));
int elf_filepos(int task, void *linear, unsigned int *outpos, unsigned int *outsize, int *perms, int *page_displacement);

int elf_check_header(int task);
int elf_check(int task);

/* Page alocation defines */
#define PA_REGIONSIZE		(0x400000)										// size of regions assigned for stealing/aging = 1 page table (4MB)
#define POOL_AVAILABLE		(pool_megabytes * 0x100000 - POOL_TABLES_SIZE)	// available pool memory
#define PA_REGIONS_COUNT	(POOL_AVAILABLE / PA_REGIONSIZE)				// regions on pool available

/* Initial page age. */
#define PA_INITIAL_AGE  4	

/* Page allocation functions */
void pa_init();
int pa_init_process_msg();
void init_swap_bitmap();
void pa_init_data();
void pa_page_taken();
void pa_page_added();
int check_page_swap(int task_id, int thread_id, void *linear);
void pa_check_swap_msg();
void swap_free_addr(unsigned int swap_addr);
void swap_use_addr(unsigned int swap_addr);
unsigned int swap_get_addr();
void task_swap_empty(int task_id);

void pa_age_pages();
void pa_steal_pages();
void steal_finish_io();
unsigned int calculate_points(unsigned int tbl_entry, unsigned int *tbl_ptr, int ioavailable);

extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

void pman_print(char *str, ...);
void pman_print_and_stop(char *str, ...);

#endif /* __PMAN_INTERNALS_HEADER */
