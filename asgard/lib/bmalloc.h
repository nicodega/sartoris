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

#include <lib/const.h>

#define MEM_TBL_ENTRIES              53              // entries per table 
#define MEM_BUCKETS                  16              // possible bucket sizes
#define MEM_CHUNK_SIZE               0x200000        // size of buckets chunks 2^21 (must be a power of 2)
#define MEM_TABLES                   20              // tables 
#define MEM_TABLES_RESERVED_PAGES    MEM_TABLES
#define MEM_TABLES_RESERVED_SIZE     (0x1000 * MEM_TABLES)    // 80 KB
#define MEM_MAGIC                    0xDEADDEAD
#define MEM_TOTAL_CHUNKS             0x400                    // MAX 2 GB
#define MEM_CHUNKS_BITMAP            (MEM_TOTAL_CHUNKS/8)
#define MEM_CHUNKS_START             (_mem.mem_start + MEM_TABLES_RESERVED_SIZE)
#define MEM_FIRST_CHUNK              (_mem.mem_start + MEM_TABLES_RESERVED_SIZE)
#define MEM_ALLOCATED_CHUNKS         1
#define MEM_MAX_UNIT                 0x100000                 // 1 MB
#define MEM_BUCKET_INDEX(bucket)     (_mem_get_bucket_index(bucket))
#define MEM_SIZE_BUCKET_INDEX(bucket)     (_mem_get_size_bucket_index(bucket))

unsigned char pg_bitmap[MEM_CHUNKS_BITMAP];	

#define MIN(a,b) (((unsigned int)a < (unsigned int)b)? (unsigned int)a : (unsigned int)b)

struct _mem_bucket
{
	unsigned int size;
	struct _mem_bucket *next;
	struct _mem_bucket *prev;
} PACKED_ATT;

struct _mem_taken_bucket
{
	unsigned int size;
	struct _mem_chunk *chunk;	// pointer to the bucket containing chunk
	unsigned int magic;			// 0xDEADDEAD 
} PACKED_ATT;

/* A memory chunk */
struct _mem_chunk
{
	unsigned int start;
	unsigned int end;
	unsigned int length;
	struct _mem_bucket *free_bucket[MEM_BUCKETS];		// this will hold a pointer to the first free bucket for each size
} PACKED_ATT; // 19 dwords

/* Allocated chunks table (4096 bytes) */
struct _mem_chunk_tbl
{
	struct _mem_chunk entries[MEM_TBL_ENTRIES];
} PACKED_ATT;

/* Main memory structure */
struct _mem_mem
{
	unsigned int mem_start;	   // bss end
	unsigned int mem_end;	   // max address
	unsigned int alloc_mem;    // bytes allocated (real)
	unsigned int alloc_chunks; // allocated chunks
	unsigned int free_table;   // first table with free chunks
	struct _mem_chunk_tbl *chunk_tbls[MEM_TABLES];	
} _mem;

/* 
Must return 0 if successful; otherwise the value -1 is 
returned and the global variable errno is set  
*/
extern int brk(const void *addr);
/* 
increments incr bytes the current brk and returns the max address.
Returns the prior break value if successful; other-
wise the value (void *)-1 is returned and the global variable errno is set 
*/
extern void *sbrk(int incr);

int _mem_get_bucket_index(unsigned int size);
void _mem_bbmp_take(unsigned int chunk_index, unsigned int chunks);
void _mem_bbmp_ret(unsigned int chunk_address, unsigned int chunks);
int _mem_bbmp_find(unsigned int chunks);
void _zero(void *ptr, unsigned int length);
unsigned int _mem_find_bucket(struct _mem_chunk *chunk, unsigned int size, int *available_bucket);
struct _mem_taken_bucket *_mem_get_bucket(struct _mem_chunk *chunk, unsigned int bucket, int i);
void _mem_put_bucket(struct _mem_taken_bucket *t);
void _mem_init_chunk(struct _mem_chunk *chunk, unsigned int start, unsigned int length, BOOL bucketed);
void _mem_init(unsigned int bssend, unsigned int maxaddress);
void _mem_init_tbl(struct _mem_chunk_tbl *tbl);
struct _mem_chunk_tbl *_mem_allocate_tbl();
struct _mem_chunk *_mem_allocate_chunk(unsigned int size, int tbl_index, BOOL bucketed);
void *_mem_alloc(unsigned int size, int zero);
void _mem_free(void *ptr);

/* 
Must return 0 if successful; otherwise the value -1 is 
returned and the global variable errno is set  
*/
extern int brk(const void *addr);
/* 
increments incr bytes the current brk and returns the max address.
Returns the prior break value if successful; other-
wise the value (void *)-1 is returned and the global variable errno is set 
*/
extern void *sbrk(int incr);


