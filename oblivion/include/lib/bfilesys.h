/*
 *   basicFAT file system Library headers v0.2
 *
 *   Copyright (C) 2002, Matias Brustein Macri
 *
 *   mbmacri@dc.uba.ar
 */

struct fatline
{
	char	filename[50];
	void*	filepos;
	int	filesize;
};

void readfat(void *data, int *filecounter, struct fatline table[]);

