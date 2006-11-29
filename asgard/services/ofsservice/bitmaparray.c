/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
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

#include "bitmaparray.h"
#include <lib/const.h>
#include <lib/malloc.h>
#include <services/ofsservice/ofs.h>


int init_bitmaparray(bitmap *bitmap, char* bits, int max_value, int blockdev_blocks, unsigned int index_offset, unsigned int multiplier)
{
	int i = 0, modified_size, j;
	unsigned int k;
	int max_byte = 0;
	

	if(bits == NULL) return -1;

	bitmap->max_value = max_value;
	bitmap->bits = bits;
	bitmap->offset = index_offset;
	bitmap->multiplier = multiplier;

	modified_size = (int)(blockdev_blocks / 8) + ((blockdev_blocks % 8 == 0)? 0 : 1);

	bitmap->modified_length = modified_size;
	bitmap->first_modified = modified_size;
	bitmap->last_modified = -1;
	
	// create modified array
	bitmap->modified = (char *)malloc(modified_size);

	while(i < modified_size)
	{
		bitmap->modified[i] = 0;
		i++;
	}

	// calculate freecount
	i = 0;
	k = 0;
	max_byte = (int)(bitmap->max_value / 8) + ((bitmap->max_value % 8 == 0)? 0 : 1);
	bitmap->freecount = 0;
	while(i < max_byte)
	{
		j = 0;
		// calculate free bits on byte
		while(j < 8 && k < bitmap->max_value)
		{
			if((bitmap->bits[i] & (0x01 << j)) == 0)
			{
				bitmap->freecount++;
			}
			j++;
			k++;
		}
		i++;
	}

	return 0;
}

unsigned int get_free_index(bitmap *bitmap)
{
	unsigned int i = 0, j = 0, k = 0, free_index, modified_block;
	unsigned int max_byte = (int)(bitmap->max_value / 8) + ((bitmap->max_value % 8 == 0)? 0 : 1);
	char *bits = bitmap->bits;

	if(bitmap->freecount == 0) return -1;

	while(i < max_byte)
	{
		if((bitmap->bits[i] & 0xFF) != 0xFF)
		{
			j = 7;
			// find a free slot
			while(j >= 0 && k < bitmap->max_value)
			{
				if((bitmap->bits[i] & (0x01 << j)) == 0)
				{
					bitmap->bits[i] = bitmap->bits[i] | (0x01 << j);
					bitmap->freecount--;
					free_index = i * 8 + 8 - j;

					modified_block = free_index / OFS_BLOCKDEV_BLOCKSIZE;

					// set free_index block as modified
					bitmap->modified[modified_block / 8] = bitmap->modified[modified_block / 8] | (0x80 >> (modified_block % 8));
					if(bitmap->first_modified > (modified_block / 8))
					{
						bitmap->first_modified = (modified_block / 8);
					}
					if(bitmap->last_modified < (modified_block / 8) || bitmap->last_modified == (unsigned int)-1)
					{
						bitmap->last_modified = (modified_block / 8);
					}

					return free_index * bitmap->multiplier + bitmap->offset;
				}
				j--;
			}
		}
		k += 8;
		i++;
	}

	return -1;
}

unsigned int *get_free_indexes(bitmap *bitmap, int count)
{
	unsigned int i = 0, j = 0, k = 0, current_block = -1;
	unsigned int max_byte = (int)(bitmap->max_value / 8) + ((bitmap->max_value % 8 == 0)? 0 : 1);
	char *bits = bitmap->bits;
	unsigned int *indexes = (unsigned int *)NULL;

	if(bitmap->freecount < count) return (unsigned int *)NULL;

	indexes = (unsigned int *)malloc(sizeof(unsigned int) * count);

	while(i < max_byte)
	{
		if((bitmap->bits[i] & 0xFF) != 0xFF)
		{
			j = 7;
			// find a free slot (from left to right)
			while(j >= 0 && k < bitmap->max_value)
			{
				if((bitmap->bits[i] & (0x01 << j)) == 0)
				{
					bitmap->bits[i] = bitmap->bits[i] | (0x01 << j);
					bitmap->freecount--;

					count--;
					indexes[count] = i * 8 + 8 - j - 1;

					if(indexes[count] / OFS_BLOCKDEV_BLOCKSIZE != current_block)
					{
						current_block = indexes[count] / OFS_BLOCKDEV_BLOCKSIZE;

						// set block as modified
						bitmap->modified[current_block / 8] = bitmap->modified[current_block / 8] | (0x80 >> (current_block % 8));
						if(bitmap->first_modified > (current_block / 8))
						{
							bitmap->first_modified = (current_block / 8);
						}
						if(bitmap->last_modified < (current_block / 8) || bitmap->last_modified == (unsigned int)-1)
						{
							bitmap->last_modified = (current_block / 8);
						}
					}

					indexes[count] = indexes[count] * bitmap->multiplier + bitmap->offset; 

					if(count == 0)
					{
						return indexes;
					}
				}
				j--;
			}
		}
		i++;
		k += 8;
	}

	return (unsigned int *)NULL;
}

void free_indexes(bitmap *bitmap, unsigned int *indexes, int length)
{
	int i = 0;

	while(i < length)
	{
		free_index(bitmap, (indexes[i] - bitmap->offset) / bitmap->multiplier);
		i++;
	}
}

int free_index(bitmap *bitmap, unsigned int index)
{
	unsigned int i = 0, j = 0, modified_block = 0;
	unsigned int real_index = (index - bitmap->offset) / bitmap->multiplier;

	i = (int)(real_index / 8);
	j = real_index % 8;
	modified_block = real_index / OFS_BLOCKDEV_BLOCKSIZE;

	if(real_index < 0 || i > bitmap->max_value) return -1;

	bitmap->bits[i] = (bitmap->bits[i] & (~(0x80 >> j)));

	bitmap->freecount++;

	// set index block as modified
	bitmap->modified[modified_block / 8] = bitmap->modified[modified_block / 8] | (0x80 >> (modified_block % 8));
	if(bitmap->first_modified > (modified_block / 8))
	{
		bitmap->first_modified = (modified_block / 8);
	}
	if(bitmap->last_modified < (modified_block / 8) || bitmap->last_modified == (unsigned int)-1)
	{
		bitmap->last_modified = (modified_block / 8);
	}

	return 0;
}

int free_bitmap_array(bitmap *bitmap)
{
	free(bitmap->bits);
	free(bitmap->modified);
	return 0;
}

void reset_modified(bitmap *bitmap, int block)
{
	// NOTE: this function does not preseve first_modified and last_modified
	// it's up to the caller to preserve them if needed
	bitmap->modified[block / 8] = bitmap->modified[block / 8] & (~(0x80 >> (block % 8)));
}

unsigned int get_first_modified(bitmap *bitmap, int modified_index, int first_block)
{
	unsigned int j = 0;

	if(first_block != -1){ j = first_block % 8 + 1; }

	while(j < 8)
	{
		if((bitmap->modified[modified_index] & (0x80 >> j)) != 0) return modified_index * 8 + j; // return the id of the modified block
		j++;
	}

	return -1;
}

