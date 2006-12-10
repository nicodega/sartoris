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
/*
	Dynamic buffer malloc implementation.

	This malloc system will use a chunk aproach. Chunks will have 2MB by default. 

	We will mantain a variable size buffer, starting at bss end, and ending on max address, reported by pman. The algorithm, 
	will dinamically resize (either shrink o enlarge the buffer), according to memory needs.

	- Small allocations, will be taken from a chunk, in a buddy fashion, using powers of 2.
	- Big allocations will be performed by generating larger chunks.

	Sizes considered for buddy will be:  2^6  to MEM_CHUNK_SIZE

	MAX DYNAMIC MEMORY POOL SIZE ON THIS IMPLEMENTATION WILL BE ~ 2 GB

	SOME THINGS ARE REALLY INNEFICIENT... BUT THIS IS MENT TO BE JUST BETTER THAN 
	OUR PREVIOUS MALLOC

	TODO: Try to reduce page faults made by creating buddies the first time...
	We could use a bitmap like linux does, this would give us 2 page faults for
	each chunk, but search for a free buddie will be slower.

*/

#include "bmalloc.h"
#include <lib/malloc.h>

#ifdef SAFE
static struct mutex malloc_mutex;	// for safe threading malloc
#endif

unsigned int bucket_size[] = {0x40,    0x80,    0x100,   0x200,   0x400, 
						      0x800,   0x1000,  0x2000,  0x4000,  0x8000,  
							  0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 
							  0x200000};

/* Returns for a given size, it's bucket index */
int _mem_get_size_bucket_index(unsigned int size)
{
	unsigned int s = 64;
	int i = 0;

	while(s <= MEM_MAX_UNIT)
	{
		if(size < s - sizeof(struct _mem_bucket)) return i;
		s = (s << 1);
		i++;
	}
	return -1;
}
/* Returns given a bucket size, it's index */
int _mem_get_bucket_index(unsigned int size)
{
	unsigned int s = 64;
	int i = 0;

	while(s <= MEM_CHUNK_SIZE)
	{
		if(size == s) return i;
		s = (s << 1);
		i++;
	}
	return -1;
}
/* Mark chunk as taken on bitmap */
void _mem_bbmp_take(unsigned int chunk_index, unsigned int chunks)
{
	int i, j;

	j = chunk_index % 8;
	i = (unsigned int)((chunk_index-j) / 8);

    while(chunks > 0)
	{
		while(chunks > 0)
		{
			pg_bitmap[i] &= ~(0x1 << (7 - j));	
			j++;
			chunks--;
		}		
		j = 0;		
		i++;
	}
}
/* Return chunk addresses to bitmap */
void _mem_bbmp_ret(unsigned int chunk_address, unsigned int chunks)
{
	int i, j;

	i = (unsigned int)((chunk_address - MEM_CHUNKS_START) / MEM_CHUNK_SIZE);
	j = i % 8;
	i = (unsigned int)((i-j) / 8); 

    while(chunks > 0)
	{
		while(chunks > 0)
		{
			pg_bitmap[i] |= (0x1 << (7 - j));	
			j++;
			chunks--;
		}
		j = 0;				
		i++;
	}
}
/* Find a free chunk address succession on the bitmap to hold size bytes */
int _mem_bbmp_find(unsigned int chunks)
{
	int i = 0, j = 0;
	int cont = 0, candidate_i = -1, candidate_j = 0;
	
	while(i < MEM_CHUNKS_BITMAP)
	{
		/* check if there are free chunks here */
		if(pg_bitmap[i])
		{
            j = 7;
			while(j >= 0)
			{
				if(pg_bitmap[i] & (0x1 << j))
				{
					if(candidate_i == -1)
					{
						if(chunks == 1) return i * 8 + (7 - j);
						candidate_i = i;
						candidate_j = j;
						cont = 0;						
					}
					else
					{
						cont++;
						if(chunks == cont) return candidate_i * 8 + (7 - candidate_j);
					}
				}
				else if(candidate_i != -1)
				{
					candidate_i = -1;
				}
				j--;
			}
		}
		i++;
	}

	return -1;
}
/* zero out memory */
void _zero(void *ptr, unsigned int length)
{
	unsigned int i = 0;
	unsigned char *p = (unsigned char*)ptr;
	/* awfoul implementation */
	for(i =0;i<length;i++){p[i]=0;}
}
/*Find the best bucket on the chunk, we can provide */
unsigned int _mem_find_bucket(struct _mem_chunk *chunk, unsigned int size, int *available_bucket)
{
	int i = MEM_SIZE_BUCKET_INDEX(size), j;
	j = i;

	while(i < MEM_BUCKETS && chunk->free_bucket[i] == NULL){i++;}

	if(i == MEM_BUCKETS) 
		return 0xFFFFFFFF;	// no buckets free big enough for this

	*available_bucket = i;
	return j;
}

/* Find a free bucket on the chunk */
struct _mem_taken_bucket *_mem_get_bucket(struct _mem_chunk *chunk, unsigned int bucket, int i)
{
	struct _mem_bucket *b, *buddy;
	struct _mem_taken_bucket *t = NULL;
	
	/* 
	We have the best biggest available chunk. Buddy buckets until we get the bucket we need!
	*/
	b = chunk->free_bucket[i];
	buddy = NULL;

	/* Remove bucket from free list */
	chunk->free_bucket[i] = chunk->free_bucket[i]->next;
	if(chunk->free_bucket[i] != NULL)
		chunk->free_bucket[i]->prev = NULL;
	
	while(i != bucket)
	{
		i--;
		/* Create a new buddy for b */
		b->size = (b->size >> 1);
		
		buddy = (struct _mem_bucket*)((unsigned int)b + b->size);
		buddy->size = b->size;
		buddy->prev = NULL;
		
		/* Insert new buddy on free list of chunk */
		if(chunk->free_bucket[i] == NULL)
		{
			buddy->next = NULL;
			chunk->free_bucket[i] = buddy;
		}
		else
		{
			chunk->free_bucket[i]->prev = buddy;
			buddy->next = chunk->free_bucket[i];
			chunk->free_bucket[i] = buddy;
		}
	}

	t = (struct _mem_taken_bucket*)b;
	t->chunk = chunk;
	t->magic = MEM_MAGIC;
	return t;
}

/* Return a taken bucket to the chunk */
void _mem_put_bucket(struct _mem_taken_bucket *t)
{
	struct _mem_bucket *b, *fbuddy;
	struct _mem_taken_bucket *buddy;
	int index;
	struct _mem_chunk *chunk = t->chunk;

	/* Check magis number */
	if(t->magic != MEM_MAGIC)
		_exit(-21);	// Trying to deallocate something we did not allocate

	/* Transform t onto a bucket */
	b = (struct _mem_bucket *)t;

	/* 
	NOTE: each bucket has a “buddy” of the same size adjacent to it, such that 
	combining a bucket of size 2^n with its buddy creates a properly aligned block of size 2^n+1
	The address of a block's buddy can be easily calculated by flipping the nth bit from the right 
	in the binary representation of the block's address. For example, the pairs of buddies 
	(0,4), (8,12), (16,20) in binary are (00000,00100), (01000,01100), (10000,10100). 
	In each case, the two addresses in the pair differ only in the third bit from the right. 
	In short, you can find the address of the buddy of a block by taking the exclusive OR of the 
	address of the block with its size. 
	*/
	buddy = (struct _mem_taken_bucket *)((((unsigned int)b - chunk->start) ^ b->size) + chunk->start);
	fbuddy = (struct _mem_bucket *)buddy;

	t->magic = 0;

	index = MEM_BUCKET_INDEX(b->size);

	while(buddy->magic != MEM_MAGIC && b->size != MEM_CHUNK_SIZE && buddy->size == b->size)
	{
		buddy->magic = 0;

		index = MEM_BUCKET_INDEX(b->size);

		/* Buddy is not allocated -> merge */
		b = (struct _mem_bucket *)MIN(b, buddy);
		
		/* Remove buddy from free list */
		if(chunk->free_bucket[index] == fbuddy)
		{
			chunk->free_bucket[index] = fbuddy->next;
			if(chunk->free_bucket[index] != NULL) 
				chunk->free_bucket[index]->prev = NULL;
		}
		else
		{
			if(fbuddy->next != NULL)
				fbuddy->next->prev = fbuddy->prev;
			if(fbuddy->prev != NULL)
				fbuddy->prev->next = fbuddy->next;
		}

		b->size = (b->size << 1);
		b->prev = NULL;
		b->next = NULL;

		index = MEM_BUCKET_INDEX(b->size);

		/* Find new node buddy */
		fbuddy = (struct _mem_bucket *)((((unsigned int)b - chunk->start) ^ b->size) + chunk->start);
		buddy = (struct _mem_taken_bucket *)fbuddy;
	}

	/* Add merged region to new free list */
	if(chunk->free_bucket[index] == NULL)
	{
		b->next = NULL;
		chunk->free_bucket[index] = b;
	}
	else
	{
		b->next = chunk->free_bucket[index];
		chunk->free_bucket[index] = b;
	}
}

/* Initialize a new chunk */
void _mem_init_chunk(struct _mem_chunk *chunk, unsigned int start, unsigned int length, BOOL bucketed)
{
	int i = 0;
	struct _mem_bucket *b;

	chunk->start = start;
	chunk->length = length;
	chunk->end = start + length;
		
	for(i=0; i < MEM_BUCKETS;i++)
		chunk->free_bucket[i] = NULL;

	b = chunk->free_bucket[MEM_BUCKETS-1] = (struct _mem_bucket *)start;
	b->size = length;
	b->next = NULL;
	b->prev = NULL;		
}

/* Initialize memory subsystem */
void _mem_init(unsigned int bssend, unsigned int maxaddress)
{
	int i = 0;
	_mem.mem_start = bssend;
	_mem.mem_end = maxaddress;
	_mem.alloc_chunks = 0;
	_mem.alloc_mem = 0;
	_mem.free_table = 0;

	for(i=0; i < MEM_TABLES;i++)
		_mem.chunk_tbls[i] = NULL;

	for(i=0; i < MEM_CHUNKS_BITMAP;i++)
		pg_bitmap[i] = 0xFF; // none taken

	/* Get enough memory for a chunk and the tables */
	if(maxaddress - bssend < MEM_TABLES_RESERVED_SIZE + MEM_CHUNK_SIZE && brk((void*)(_mem.mem_start + MEM_TABLES_RESERVED_SIZE + MEM_CHUNK_SIZE)) < 0)
			_exit(-22);

	_mem.mem_end = _mem.mem_start + MEM_TABLES_RESERVED_SIZE + MEM_CHUNK_SIZE;
}
/* Initialize buckets table */
void _mem_init_tbl(struct _mem_chunk_tbl *tbl)
{
	int i = 0;

	/* Set start on all chucks to 0xFFFFFFFF, this means it's free */
	for(i = 0; i < MEM_TBL_ENTRIES; i++)
	{
		tbl->entries[i].start = 0xFFFFFFFF;
	}
}
/* Allocate next available table */
struct _mem_chunk_tbl *_mem_allocate_tbl()
{
	int i = 0;

	/* No tables left at all? return NULL */
	if(_mem.free_table == MEM_TABLES) return NULL;

	_mem.chunk_tbls[_mem.free_table] = (struct _mem_chunk_tbl *)(_mem.mem_start + 0x1000 * _mem.free_table);

	_mem_init_tbl(_mem.chunk_tbls[_mem.free_table]);

	_mem.free_table++;

	return _mem.chunk_tbls[_mem.free_table-1];
}

/* This will get a new bucket */
struct _mem_chunk *_mem_allocate_chunk(unsigned int size, int tbl_index, BOOL bucketed)
{
	struct _mem_chunk_tbl *tbl = NULL;
	int i = 0, entry = -1, chunks = 1;
	int bmp_pos = 0;

	/* If there are no chunk tables with free chunks, get a new table */
	if(tbl_index == -1)
		tbl = _mem_allocate_tbl();
	else
		tbl = _mem.chunk_tbls[tbl_index];
	
	if(tbl == NULL) return NULL;
	
	/* Find a free chunk entry on table */
	for(i = 0; i < MEM_TBL_ENTRIES; i++)
	{
		if(tbl->entries[i].start == 0xFFFFFFFF)
		{
			entry = i;
			break;	
		}
	}

	if(bucketed)
	{
		/* Find a free chunk address on the bitmap */
		bmp_pos = _mem_bbmp_find(1);		
	}
	else
	{
		/* Find a free chunk address sucesion on the bitmap */
		int chunks = (int)(size / MEM_CHUNK_SIZE) + ((size % MEM_CHUNK_SIZE)? 1 : 0);

		bmp_pos = _mem_bbmp_find(chunks);
	}

	if(bmp_pos == -1) return NULL;

	_mem.alloc_chunks += chunks;

	if(MEM_CHUNKS_START + bmp_pos * MEM_CHUNK_SIZE >= _mem.mem_end)
	{
		if(brk((void*)(_mem.mem_end + chunks * MEM_CHUNK_SIZE)) < 0)
			_exit(-22);

		_mem.mem_end = _mem.mem_end + chunks * MEM_CHUNK_SIZE;
	}

	/* mark chunks as taken */
	_mem_bbmp_take(bmp_pos, chunks);

	_mem_init_chunk(&tbl->entries[entry], MEM_CHUNKS_START + bmp_pos * MEM_CHUNK_SIZE, chunks * MEM_CHUNK_SIZE, bucketed);
	
	return &tbl->entries[entry];
}

/* Allocation routine (called from calloc and malloc) */
void *_mem_alloc(unsigned int size, int zero)
{
	int i = 0, j = 0, tbl = -1;
	unsigned int bucket = 0xFFFFFFFF;
	int abucket;
	struct _mem_chunk *chunk = NULL;
	struct _mem_taken_bucket *t = NULL;
	void *ptr = NULL;

	/* If size is too big, allocate a specialized chunk */
	if(size >= MEM_MAX_UNIT - sizeof(struct _mem_bucket))
	{
		while(i < MEM_TABLES && tbl == -1)
		{
			if(_mem.chunk_tbls[i] != NULL)
			{
				while(j < MEM_TBL_ENTRIES)
				{
					if(tbl == -1 && _mem.chunk_tbls[i]->entries[j].start == 0xFFFFFFFF)
					{
						tbl = i;
						break;
					}
					j++;
				}
			}		
			i++;
		}

		/* Create a chunk, specially for this */
		chunk = _mem_allocate_chunk(size, tbl, FALSE);

		if(chunk == NULL)
			return NULL;

		t = (struct _mem_taken_bucket*)chunk->start;
		t->chunk = chunk;
		t->magic = MEM_MAGIC;

		_mem.alloc_mem += chunk->length;

		ptr = (void*)((unsigned int)chunk->start + sizeof(struct _mem_bucket));

		if(zero) _zero(ptr, size);

		return ptr;
	}

	/* 
	Find a chunk for us 
	NOTE: We could use another list for this... but I don't have time right now
	*/
	while(bucket == 0xFFFFFFFF && i < MEM_TABLES)
	{
		if(_mem.chunk_tbls[i] != NULL)
		{
			while(bucket == 0xFFFFFFFF && j < MEM_TBL_ENTRIES)
			{
				if(tbl == -1 && _mem.chunk_tbls[i]->entries[j].start == 0xFFFFFFFF)
					tbl = i;
				bucket = _mem_find_bucket(&_mem.chunk_tbls[i]->entries[j], size, &abucket);
				j++;
			}
		}		
		i++;
	}

	/* If we did not get a chunk,  */
	if(bucket == 0xFFFFFFFF)
	{
		chunk = _mem_allocate_chunk(MEM_CHUNK_SIZE, tbl, TRUE);

		if(chunk == NULL)
			return NULL;

		bucket = _mem_find_bucket(chunk, size, &abucket);
	}
	else
	{
		chunk = &_mem.chunk_tbls[i-1]->entries[j-1];
	}

	/* Take the bucket */
	t = _mem_get_bucket(chunk, bucket, abucket);

	_mem.alloc_mem += t->size;

	ptr = (void*)((unsigned int)t + sizeof(struct _mem_taken_bucket));

	if(zero) _zero(ptr, size);

	return ptr;
}


/* free function (implements free) */
void _mem_free(void *ptr)
{
	struct _mem_taken_bucket *t = (struct _mem_taken_bucket *)((unsigned int)ptr - sizeof(struct _mem_taken_bucket)), *t2;
	struct _mem_chunk *chunk = NULL;
	unsigned int bg_start = 0;
	int i = 0, j = 0, chunks;

	/* Check magic */
	if(t->magic != MEM_MAGIC) 
		_exit(-24);

	chunk = t->chunk;
	_mem.alloc_mem -= t->size;
	
	/* 
	Get biggest chunk address.
	What I'll do here is awful... but this is just a temporary implementation
	of a better malloc.
	*/

	while(i < MEM_TABLES)
	{
		if(_mem.chunk_tbls[i] != NULL)
		{
			while(j < MEM_TBL_ENTRIES)
			{
				if(_mem.chunk_tbls[i]->entries[j].start != 0xFFFFFFFF && _mem.chunk_tbls[i]->entries[j].start > bg_start)
				{
					bg_start = _mem.chunk_tbls[i]->entries[j].start;
				}
				j++;
			}
		}		
		i++;
	}

	if(bg_start < _mem.mem_start + MEM_TABLES_RESERVED_SIZE + MEM_CHUNK_SIZE * MEM_ALLOCATED_CHUNKS)
		bg_start = 0;

	if(t->size > MEM_MAX_UNIT)
	{
		chunks = (unsigned int)(t->size / MEM_CHUNK_SIZE);
		/* Specialized chunk, return chunks on bitmap */
		_mem_bbmp_ret(chunk->start, chunks);

		/* chall we brk? */
		if(chunk->start == bg_start)
		{
			if(brk((void*)chunk->start) < 0)
				_exit(-22);
		}
		chunk->start = 0xFFFFFFFF;    // chunk entry no longer taken
		_mem.alloc_chunks -= chunks;
	}
	else
	{
		/* return space on chunk */
		_mem_put_bucket(t);

		chunks = 1;
		t2 = (struct _mem_taken_bucket *)chunk->start;

		/* Check if chunk is completely free */
		if(t2->magic != MEM_MAGIC && t2->size == MEM_CHUNK_SIZE)
		{
			/* return chunk on bitmap */
			_mem_bbmp_ret(chunk->start, 1);

			/* 
			Ok, it's free, shall we brk? 
			In order to brk back, we must know whick is the higher chunk allocated			
			*/
			if(chunk->start == bg_start)
			{
				if(brk((void*)chunk->start) < 0)
					_exit(-22);
			}

			chunk->start = 0xFFFFFFFF;    // chunk entry no longer taken
			_mem.alloc_chunks -= chunks;
		}
	}	
}


/* support for malloc.h */
void init_mem(void *buffer, unsigned int size)
{
#	ifdef SAFE
	init_mutex(&malloc_mutex);
#	endif
	_mem_init((unsigned int)buffer, (unsigned int)buffer + size);
}

void close_malloc_mutex()
{
#	ifdef SAFE
	close_mutex(&malloc_mutex);
#	endif
}

void *malloc(size_t size)
{
	void *ptr = NULL;
#	ifdef SAFE
	wait_mutex(&malloc_mutex);
#	endif
	ptr =  _mem_alloc(size, 0);
#	ifdef SAFE
	leave_mutex(&malloc_mutex);
#	endif
	return ptr;
}

void *calloc(size_t nelem, size_t elsize)
{
	void *ptr = NULL;
#	ifdef SAFE
	wait_mutex(&malloc_mutex);
#	endif
	ptr =  _mem_alloc(nelem * elsize, 1);
#	ifdef SAFE
	leave_mutex(&malloc_mutex);
#	endif
	return ptr;
}

void free(void *ptr)
{
#	ifdef SAFE
	wait_mutex(&malloc_mutex);
#	endif
	_mem_free(ptr);
#	ifdef SAFE
	leave_mutex(&malloc_mutex);
#	endif
}

// returns free memory
unsigned int free_mem()
{	
    return (_mem.mem_end - _mem.mem_start) - _mem.alloc_mem;
}

int brk(const void *addr) __attribute__ ((weak));
void *sbrk(int incr) __attribute__ ((weak));
void _exit(int code) __attribute__ ((weak));

void _exit(int code)
{
	for(;;);
}

/* Compatibility implementations until we have break */
int brk(const void *addr)
{
	return -1;
}

void *sbrk(int incr)
{
	return (void *)-1;
}
