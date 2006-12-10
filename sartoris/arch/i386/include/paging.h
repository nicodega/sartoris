#ifndef __PTABLES_H
#define __PTABLES_H

#ifdef PAGING

typedef unsigned int pt_entry;
typedef unsigned int pd_entry;

/* linear address used to map pages from
   other tasks (should be otherwise unused) */
#define AUX_MAPPING_BASE 0x300000	/* linear */
#define AUX_PAGE_SLOT(thread) (void*)(AUX_MAPPING_BASE + thread * PG_SIZE)

#define PG_LINEAR_TO_DIR(linear) ((unsigned int)linear >> 22)
#define PG_LINEAR_TO_TAB(linear) (((unsigned int)linear & 0x003fffff) >> 12)
#define PG_LINEAR_TO_OFFSET(linear) ((unsigned int)linear & 0x00000fff)

#define PG_ADDRESS(physical) ((unsigned int)physical & 0xfffff000)

/* page table and page directory option bits */

/* defined by Intel: */
#define PG_PRESENT   0x00000001
#define PG_WRITABLE  0x00000002 
#define PG_USER      0x00000004   
#define PG_WRITE_THR 0x00000008
#define PG_CACHE_DIS 0x00000010
#define PG_ACCESSED  0x00000020
#define PG_DIRTY     0x00000040  /* only defined for p-table entries     */
#define PG_IS_4MB    0x00000080  /* only defined for p-directory entries */ 
#define PG_GLOBAL    0x00000100

/* defined by us: */
#define PG_SHARED    0x00000200 /* the page belongs to at least one SMOs */
#define PG_MAPPED    0x00000400 /* this entry was created to map a SMO   */

struct i386_task;

void init_paging(void);
void start_paging(struct i386_task *tinf);

int import_page(int task, void *linear);
void *arch_translate(int task, void *address);

int verify_present(void *address, int write);

void static map_page(void *linear);
void static invalidate_tlb(void *linear);

#endif

#endif
