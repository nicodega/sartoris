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
Kernel Malloc routine:

This routine will be based on a Fixed Size bucket allocation.

kmem
	-> Directory (< 28) -> Directory -> Directory
		-> Page Descriptors[PAGE_DESCRIPTORS]
			-> Page (Here is where data is located)
	-> Directory (29 - 60)
	-> Directory (61 - 124)
	-> Directory (125 - 252)
	-> Directory (125 - 252)
	-> Directory (253 - 508)
	-> Directory (509 - 1020)
	-> Directory (1021 - 2044)
*/

#include "types.h"

#ifndef KMALLOCH
#define KMALLOCH

/*
	A page assigned as a bucket container. (Buckets are where data is located)
*/
struct kmem_page
{
	BYTE buckets[0x1000];
} PACKED_ATT;

struct kmem_directory_page;
struct kmem_free_node;

/*
	Information for bucket pages.
*/
struct kmem_page_descriptor
{
	UINT16 bucket_size;
	UINT16 flags;
	struct kmem_page *page;

	/* Free Buckets List */
	BYTE free_buckets;
	BYTE index;
	struct kmem_free_node *free_first;
	
	/* Linked descriptor list */
	struct kmem_page_descriptor *next_descriptor;
	struct kmem_page_descriptor *prev_descriptor;

	/* Parent directory */
	struct kmem_directory_page *directory;
} PACKED_ATT; // 26 bytes

#define KMEM_DFLAG_ALLOCATED 0x1
#define KMEM_DFLAG_NONE		 0x0

/*
	Taken header (each allocated bucket will have this structure at the begining)
*/
struct kmem_header
{
	struct kmem_page_descriptor *descriptor;
	UINT32 magic_preserve;
} PACKED_ATT;

/*
	Free node header (each non-allocated bucket will have this structure at the begining)
*/
struct kmem_free_node 
{
	struct kmem_free_node *next_free;
	UINT32 magic;
} PACKED_ATT;

#define PAGE_DESCRIPTORS 156

/*
	A page assigned as a descriptor container for buckets.
*/
struct kmem_directory_page
{
	UINT32 allocated_descriptors;							// Total Descriptors allocated
	
	struct kmem_page_descriptor *first_descriptor;			// First Allocated descriptor 
	struct kmem_page_descriptor *first_na_descriptor;		// First non-allocated descriptor 
	
	/* Directory linked list */
	struct kmem_directory_page *next;
	struct kmem_directory_page *prev;

	struct kmem_page_descriptor d[PAGE_DESCRIPTORS];
	unsigned int magic;
	unsigned int free_alloc_descriptors; // NOT USED... but can be used to improve performance (will prevent some pageout/pagein)
	UINT32 padding[3];
} PACKED_ATT;

#define BUCKET_SIZE_INDEX(bsize) ( ((bsize == 32)? 0 : ((bsize == 64)? 1 : ((bsize == 128)? 2 : ((bsize == 256)? 3 : ((bsize == 512)? 4 : ((bsize == 1024)? 5 : ((bsize == 2048)? 6 : -1))))))) )
#define BUCKET_SIZE(bsize) ( ((bsize <= 24)? 32 : ((bsize <= 56)? 64 : ((bsize <= 120)? 128 : ((bsize <= 248)? 256 : ((bsize <= 504)? 512 : ((bsize <= 1016)? 1024 : ((bsize <= 2040)? 2048 : -1))))))) )

struct kmem
{
	/* Directories for bucket sizes */
	struct kmem_directory_page *directory[7];
	UINT32 total_directories[7];	// Total directories allocated
	UINT32 free_directories[7];	// Total directories allocated

	/* Statistics */
	UINT32 total_pages;				// Total pages allocated for memory management
	UINT32 total_mem;				// Total space used on all pages
	UINT32 fixed_pages;				// How many Pages we were granted before hand. (always available)
	UINT32 fixed_pages_start;		// Pages we were granted before hand. (always available)
} PACKED_ATT;

void kmem_init(UINT32 start_mem, UINT32 start_pages);
void *kmalloc(UINT32 size);
void kfree(ADDR ptr);


#endif
