/*  
 *   Sartoris main kernel header                                          
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <sartoris/types.h>

/* limitations and init params */
#define MAX_ALLOC_LINEAR   0xC800000   /* 200 MB () */
#define MIN_TASK_OFFSET    MAX_ALLOC_LINEAR

#define MAX_SCA						36    /* max system calls */
#define MAX_TSK						4096  /* max tasks */
#define MAX_THR						8192  /* max concurrent threads of execution */
#define MAX_IRQ						64    /* max irqs */
#define MAX_SMO						32768 /* system-wide */
#define MAX_MSG						32768 /* system-wide */
#define MAX_OPEN_PORTS				(32*MAX_TSK)   /* system-wide */
#define MAX_TSK_OPEN_PORTS			32
#define MAX_MSG_ON_PORT				64 

#define MAX_NESTED_INT 32

#define INIT_TASK_NUM   31   /* INIT_TASK_NUM must NOT be zero                */ 
#define INIT_THREAD_NUM 31   /* (zero is used as pre-init pseudo-task number) */

#ifndef NULL
# define NULL 0
#endif

/* possible task states */
enum task_state { LOADING=0, ALIVE, UNLOADING, DEFUNCT };

/* thread run mode options */
enum usage_mode { PERM_REQ=0, PRIV_LEVEL_ONLY, DISABLED, UNRESTRICTED };

#define MAX_USAGE_MODE  UNRESTRICTED

/* shared-memory object permissions */
#define READ_PERM  2 
#define WRITE_PERM 4

#define SUCCESS    0
#define FAILURE   -1

/* Don't move this include to the top. It uses MAX_THR. */
#ifdef __KERNEL__
#include <kernel-arch.h>
#endif

/* Structures pre definition */
struct c_task_unit;			// this structure comes from containers.h
struct c_thread_unit;		// this structure comes from containers.h
struct port;
struct smo;

/* main data structures */
struct task 
{
	void *mem_adr;
	unsigned int size;
	int priv_level;

#ifdef __KERNEL__
	char state;         
	char padding[3];
	int thread_count;
	/* 
	Queue  mapping, it binds an open port to a message queue from the 
    message queue pool 
	*/
	struct port *open_ports[MAX_TSK_OPEN_PORTS];
	struct smo *first_smo;
	int smos;
#endif
} __attribute__ ((__packed__));

struct thread 
{
	int task_num;
	int invoke_mode;
	int invoke_level;
	void *ep;
	void *stack;
    
#ifdef __KERNEL__
    struct thread_perms *run_perms;     // this is a pointer to a user space page!
    char page_faulted;					/* used to know if we have produced a page fault */
    char padding;
	short last_error;                   // last error (see error.h)
	struct c_thread_unit *next_free;	// used only when free	
#endif
} __attribute__ ((__packed__));

struct page_fault 
{
	int task_id;
	int thread_id;
	void *linear;
	int pg_size;	    // used if our architecture provides multiple page sizes.
	int flags;			// flags for the page fault
};

/* 
This is an optional access permission structure for threads
which implement the PERM_REQ access mode. 
*/
struct thread_perms
{
    int length; // next to this, must be the bitmap
};

/* 
This is an optional access permission structure for ports
which implement the PERM_REQ access mode.
*/
struct port_perms
{
    int length; // next to this, must be the bitmap
};

#define PF_FLAG_NONE 0  // a normal page fault interrupt.
#define PF_FLAG_FREE 1  // a page fault returning a page from the kernel.
#define PF_FLAG_PG   2  // a page fault for a kernel page need.
#define PF_FLAG_PGS(a) (a-1) // a page fault for the kernel asking for X-1 pages

#ifdef __KERNEL__

void handle_int(int number);

/* a default VALIDATE_PTR: */
#ifndef VALIDATE_PTR
#define VALIDATE_PTR(x) ((unsigned int) (x) < (unsigned int) (GET_PTR(curr_task,tsk))->size)
#endif
/* A check for unsigned sum overflow */
#ifndef SUMOVERFLOW
#define SUMOVERFLOW(x, y) (((unsigned int)x+(unsigned int)y) < (unsigned int)x || ((unsigned int)x+(unsigned int)y) < (unsigned int)y)
#endif

/* verify that the basic memory layout defines are present */

#ifndef KRN_OFFSET
#error "Architecture does not define the kernel offset in memory (KRN_OFFSET)"
#endif

#ifndef USER_OFFSET
#error "Architecture does not define the offset at which userspace begins (USER_OFFSET)"
#endif

#endif /* __KERNEL__ */

/* these defines can't be moved into PAGING because
   when the files are used from userland kernel-arch.h
   can't be included */
   
/* or can it be included? */
#define PGATT_CACHE_DIS 1
#define PGATT_WRITE_ENA 2
#define PGATT_WRITE_THR 4

#ifdef PAGING

#ifdef __KERNEL__ /* things we only check when compiling the kernel */

#ifndef PG_SIZE
#error "Paging architecture does not define a default page size (PG_SIZE)"
#endif

#ifndef PG_LEVELS
#error "Paging architecture does not define the number of page-indirections (PG_LEVELS)"
#endif

#ifndef IS_PAGE_FAULT
#error "Architecture does not define IS_PAGE_FAULT predicate"
#endif

#endif /* __KERNEL__ */

#endif /* PAGING */

#endif /* _KERNEL_H_ */



