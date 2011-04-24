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

#include <lib/printf.h>
#include <lib/iolib.h>
#include <lib/malloc.h>

char *invalid_params = "Invalid parameters.\n\0\0";

#define MIN(a,b) ((a < b)? a : b)

// shows the contents of a file
// on console
// usage: cat filename
int main(int argc, char **args)
{
	FILE *f;
	FILEINFO *finf;
	char buffer[132];
	int s;
	char *path = NULL;

	if(argc != 2)
	{
		printf(invalid_params);
		printf("Usage: cat filename.\n");
		return;
	}

	path = args[1];
		
	finf = finfo(path);

	if(finf == NULL)
	{
		free(path);
		printf(mapioerr(ioerror()));
		printf("\n");
		return;
	}

	if(finf->file_type == IOLIB_FILETYPE_DIRECTORY)
	{
		free(path);
		printf("Cannot cat directories.\n");
		return;
	}

	f = fopen(path, "r");

	if(f == NULL)
	{
		free(path);
		printf(mapioerr(ioerror()));
		printf("\n");
		return;
	}

	while(finf->file_size > 0)
	{
		s = MIN(129, (unsigned int)finf->file_size);
		if(fread(buffer, 1, s, f) != s)
		{
			fclose(f);
			free(path);
			printf(mapioerr(ioerror()));
			printf("\n");
			return;
		}
		buffer[s] = '\0';
		printf(buffer);
		finf->file_size -= s;
	}

	fclose(f);
	
	free(finf);
	free(path);

	printf("\n");
}
