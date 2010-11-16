/*
 *   basicFAT file system Library v0.2
 *
 *   Copyright (C) 2002, Matias Brustein Macri
 *   
 *   mbmacri@dc.uba.ar
 */


#include <lib/bfilesys.h>

void cpfatline (struct fatline *dest, void *source)
{
	int i;
	dest->filesize = ((struct fatline*)source)->filesize;
	dest->filepos =  ((struct fatline*)source)->filepos;
	for ( i = 0; i < 50; i++)
	{
		dest->filename[i] = ((struct fatline*)source)->filename[i];
	}
}

void readfat(void *data, int *filecounter, struct fatline table[])
{
	int i;
	void *bfatentrypoint;
	
	/* remember the bFAT table entry point */
	bfatentrypoint= data;

	/* get the number of files */
	*filecounter = *((int*)data);
	data = (void*)((int)data + sizeof(int));
	
	/* get all the bFAT entries */
	for ( i=0; i < *filecounter; i++)
	{
		cpfatline(&table[i], data);
		table[i].filepos = (void*)((int)table[i].filepos + (int)bfatentrypoint);
		data = (void*) ((int)data + sizeof(struct fatline));
	}
}
