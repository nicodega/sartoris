/*
*
* An ls command for the shell
*
*/

#include "shell_iternals.h"
#include <services/ofsservice/ofs.h>

// usage: ls path
void ls(int term, char **args, int argc)
{
	FILE *f;
	struct sdir_entry dentry;
	char buffer[BUFFER_SIZE];
	char *path;
	int read, total_files = 0;
	int ftype, plen, j, show_hidden = 0, show_info = 0,i=0;
	FILEINFO *finf = NULL;

	if(argc > 3 
		|| ( argc == 2 && !streq(args[0], "-h") && !streq(args[0], "-i" ) )
		|| ( argc == 3 && 
				!( (streq(args[0], "-h") && streq(args[1], "-i" ) )
				|| (streq(args[1], "-h") && streq(args[0], "-i" ) )  ) ) )
	{
		term_color_print(term, "\nInvalid parameters.\n", 12);
		term_color_print(term, "Usage: ls [-h|-i] [path]\n", 7);
		return;
	}
	else if(argc == 1 && !streq(args[0], "-h") && !streq(args[0], "-i") )
	{
		path = build_path(args[0], term);
	}
	else if(argc == 2 && !streq(args[1], "-h") && !streq(args[1], "-i" ))
	{
		path = build_path(args[1], term);
		show_hidden = 1;
	}
	else if(argc == 3)
	{
		path = build_path(args[2], term);
	}
	else
	{
		// get current path
		path = strcopy(get_env("CURRENT_PATH", term));
	}

	if( (argc >= 1 && streq(args[0], "-h")) || (argc >= 2 && streq(args[1], "-h")))
	{
		show_hidden = 1;
	}
	if( (argc >= 1 && streq(args[0], "-i")) || (argc >= 2 && streq(args[1], "-i")))
	{
		show_info = 1;
	}

	if(path == NULL)
	{
		term_color_print(term, "Invalid path.\n", 7);
	    	return;
	}

	plen = len(path);
	if(plen != 1) plen++;

	if(filetype(path) != IOLIB_FILETYPE_DIRECTORY)
	{
	    sprintf(buffer, "ls -> %s: No such directory.\n" , path);
	    term_color_print(term, buffer, 7);
	    free(path);
	    return;
	}

	f = fopen(path, "r");

	if(f == NULL)
	{
		sprintf(buffer, "ls -> %s: Could not open file. IOERR=%s\n" , path, mapioerr(ioerror()));
	    	term_color_print(term, buffer, 7);
		free(path);
		return;
	}

	sprintf(buffer, "Listing contents for directory: %s\n" , path);
	term_color_print(term, buffer, 7);
	
	// begin directory parsing
	while(!feof(f))
	{
		// read dir entry
		if(fread(&dentry, sizeof(struct sdir_entry), 1, f) != sizeof(struct sdir_entry))
		{
			if(!feof(f) || total_files != 0)
			{
				sprintf(buffer, "ls -> Unexpected end of file IOERR: %s\n" , mapioerr(ioerror()) );
				term_color_print(term, buffer, 7);
			}
			else
			{
				term_color_print(term, "Listed a total of 0 files.\n", 7);
			}
			fclose(f);
			free(path);
			return;
		}

		if((dentry.flags & OFS_DELETED_FILE) || (!show_hidden && dentry.flags & OFS_HIDDEN_FILE))
		{
			// skip this entry
			if(fseek(f, dentry.record_length - sizeof(struct sdir_entry), SEEK_CUR))
			{
				sprintf(buffer, "ls -> IOERR: %s\n" , mapioerr(ioerror()) );
				term_color_print(term, buffer, 12);
				fclose(f);
				free(path);
				return;
			}
		}
		else
		{

			read = fread(buffer, 1, MIN(BUFFER_SIZE - 1, dentry.name_length + 1), f);

			// read file name
			if(read != MIN(BUFFER_SIZE - 1, dentry.name_length + 1))
			{
				sprintf(buffer, "ls -> IOERR: %s\n" , mapioerr(ioerror()) );
				term_color_print(term, buffer, 12);
				fclose(f);
				free(path);
				return;
			}

			// print file name 
			if(dentry.name_length > BUFFER_SIZE - 1)
			{
				buffer[BUFFER_SIZE - 4] = '.';
				buffer[BUFFER_SIZE - 3] = '.';
				buffer[BUFFER_SIZE - 2] = '.';
				buffer[BUFFER_SIZE - 1] = '\0';
				ftype = -1;
			}
			else
			{
				ftype = -1;
				
				if(len(path) + len(buffer) + 1 <= BUFFER_SIZE - 2)
				{
					j = len(buffer);
					while(j >= 0)
					{
						buffer[j + plen] = buffer[j];
						j--;
					}
					estrcopy(path, buffer, 0, len(path));
					if(plen != 1) buffer[plen-1] = '/';
					if(show_info)
					{
						finf = finfo(buffer);
						if(finf != NULL) 
						{
							ftype = finf->file_type;
						}
						else
						{
							ftype = -1;
						}	
					}
					else
					{
						ftype = filetype(buffer);
					}
				}
			}

			if(!show_info || finf == NULL)
			{
				buffer[len(buffer)+1] = '\0';
				buffer[len(buffer)] = '\n';
			}

			switch(ftype)
			{
				case IOLIB_FILETYPE_FILE:
					term_color_print(term, buffer + plen, 7);
					break;
				case IOLIB_FILETYPE_DIRECTORY:
					term_color_print(term, buffer + plen, 11);
					break;
				case IOLIB_FILETYPE_DEVICE:
					term_color_print(term, buffer + plen, 9);
					break;
				default:
					term_color_print(term, buffer + plen, 12);
					break;
			}

			if(show_info && finf != NULL)
			{
				unsigned int fsize = finf->file_size;
				sprintf(buffer, "\t\t %ibytes\n", fsize);
				term_color_print(term, buffer, 7);
				free(finf);
			}

			// seek to next entry
			if(fseek(f, dentry.record_length - MIN(BUFFER_SIZE - 1, dentry.name_length + 1) - sizeof(struct sdir_entry), SEEK_CUR))
			{
				sprintf(buffer, "ls -> IOERR: %s\n" , mapioerr(ioerror()) );
				term_color_print(term, buffer, 12);
				fclose(f);
				free(path);
				return;
			}

			total_files++;
		}
	}
	fclose(f);
	free(path);
	
	sprintf(buffer, "Listed a total of %d Files\n", total_files);
	term_color_print(term, buffer, 7);

}
