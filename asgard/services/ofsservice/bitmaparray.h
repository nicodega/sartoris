/*
*	BITMAPARRAYs where deviced for handling bitmaps of nodes and blocks on OFS
*	partitions. Because of that, we asume bits will always be a multiple of
*	OFS_BLOCKDEV_BLOCKSIZE
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

#ifndef BITMAPARRAYH
#define BITMAPARRAYH



struct sbitmap
{
	unsigned int max_value;			// max value (by performing index / multiplier - offset)
	int freecount;					// free indexes left
	unsigned int modified_length;
	char* bits;		
	char* modified;					// indicates a given block is dirty
	unsigned int offset;			// incoming indexes will begin at this number
	unsigned int first_modified;	// this var will hold the first index of modified with changes
	unsigned int last_modified;		// this var will hold the last index of modified with changes
	unsigned int multiplier;
};


typedef struct sbitmap bitmap;

int init_bitmaparray(bitmap *bitmap, char* bits, int arraylength, int blockdev_blocks, unsigned int index_offset, unsigned int multiplier);
unsigned int get_free_index(bitmap *bitmap);
unsigned int *get_free_indexes(bitmap *bitmap, int count);
void free_indexes(bitmap *bitmap,unsigned int *indexes, int length);
int free_index(bitmap *bitmap, unsigned int index);
int free_bitmap_array(bitmap *bitmap);
void reset_modified(bitmap *bitmap, int block);
unsigned int get_first_modified(bitmap *bitmap, int modified_index, int first_block);


#endif

