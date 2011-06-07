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
#include <lib/sprintf.h>
#include <lib/iolib.h>
#include <lib/malloc.h>

char *invalid_params = "Invalid parameters.\n\0\0";

#define MIN(a,b) ((a < b)? a : b)

char buffer[81];
char hbuffer[80];
char hdigits[10];

// gets input from stdin and puts it on stdout
// usage: less
int main(int argc, char **args) 
{
	FILE *f, *console;
	char *path = NULL;
    int lines = 23;
    int start = 0;
    int exit = 0;
    int filename = -1;
    int hex = 0;
    int i = 1;
    
    if(ctermid(NULL) == NULL) return 0;

	if((argc == 1 && feof(&stdin)) || argc > 7)
	{
		printf(invalid_params);
		printf("Usage: more [-e] [-c] [-lines] [+linenumber] [-hex] filename.\n");
		return;
	}

    while(i < argc)
    {

        if(streq(args[i],"-e"))
        {
            exit = 1;
        }
        else if(streq(args[i],"-c"))
        {
            int j;
            // clear the console
            for(j = 0; j < 80; j++)
                buffer[j] = ' ';
            buffer[j] = '\0';
            for(j = 0; j < 25; j++)
                printf(buffer);
        }
        else if(streq(args[i],"-hex"))
        {
            hex = 1;
        }
        else if(args[i][0] == '-')
        {
            i++;
            // this should be a number
            if(!isnumeric(args[i]+1))
            {
                printf(invalid_params, 12);
		        printf("Usage: more [-e] [-c] [-lines n] [+linenumber n] [-hex] filename.\n");
		        return;
            }
            lines = atoi(args[i]+1);
        }
        else if(args[i][0] == '+')
        {
            i++;
            // this should be a number
            if(!isnumeric(args[i]+1))
            {
                printf(invalid_params, 12);
		        printf("Usage: more [-e] [-c] [-lines n] [+linenumber n] [-hex] filename.\n");
		        return;
            }
            start = atoi(args[i]+1);
        }
        else if(i == argc - 1)
        {
            path = args[i];
        }
        else
        {
            printf(invalid_params, 12);
		    printf("Usage: more [-e] [-c] [-lines n] [+linenumber n] [-hex] filename.\n");
		    return;
        }
        i++;
    }

    //printf("Reading: %s\n", path);

    if(path == NULL && feof(&stdin))
	{
		printf(invalid_params, 12);
		printf("Usage: more [-e] [-c] [-lines] [+linenumber] [-hex] filename.\n");
		return;
	}
    if(path != NULL)
    {		
        FILEINFO *finf = finfo(path);

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
		    printf("Cannot more directories.\n");
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
        
	    free(finf);
	    free(path);
    }
    else
    {
        // read from stdin
        f = &stdin;

        // open the console for fgets
        console = fopen(ctermid(NULL), "r");
    }

    int count, total = 0, read, clines = lines, cont = 0;
	while(!feof(f))
	{
        count = 0;
        do
        {
            if(hex)
            {
                read = fread(buffer, 1, 16, f);
                if(read != 16)
                {
                    if(!feof(f) || ioerror() != IOLIBERR_OK)
                    {
			            if(f != &stdin) fclose(f);
                        else fclose(console);
			            printf(mapioerr(ioerror()));
			            printf("\n");
			            return;
                    }
                    else if(!exit)
                    {
                        break;
                    }
                    else
                    {
                        if(f != &stdin) fclose(f);
                        else fclose(console);
                        return;
                    }
                }
            }
            else
            {
                if(fgets(buffer, 80, f) != buffer)
		        {        
                    if(!feof(f) || ioerror() != IOLIBERR_OK)
                    {
			            if(f != &stdin) fclose(f);
                        else fclose(console);
			            printf(mapioerr(ioerror()));
			            printf("\n");
			            return;
                    }
                    else if(!exit)
                    {
                        break;
                    }
                    else
                    {
                        if(f != &stdin) fclose(f);
                        else fclose(console);
                        return;
                    }
		        }
                read = strlen(buffer);
            }		          
                        
            if(total >= start)
            {
                if(hex)
                {   
                    int j, l, p;

                    for(j = 0; j < 81; j++)
                        hbuffer[j] = ' ';

                    l = sprintf(hbuffer, "%x ", count * 16);
                    hbuffer[l] = ' ';
                    
                    for(j = 0; j < read; j++)
                    {
                        p = j*3 + 10;
                        l = sprintf(&hbuffer[p], "%02x", (0x000000FF & (int)buffer[j]));
                        hbuffer[p + l] = ' ';
                    }
                    hbuffer[9] = ':';
                    hbuffer[80] = '\0';
                    printf(hbuffer);
                }
                else
                {
		            printf(buffer);
                }
            }
            count++;
        }while(count < clines);

        // wait for input
        char cmd;

        cont = 0;
        do
        {
            if(f != &stdin)
                cmd = fgetc(&stdin);
            else
                cmd = fgetc(console);
         
            switch(cmd)
            {
                case '\n':
                    clines = 1;
                    cont = 1;
                    break;
                case ' ':
                    // advance a whole screen
                    clines = lines;
                    cont = 1;
                    break;
                case 27:
                    if(f != &stdin)
                        fclose(f);
                    else 
                        fclose(console);
	            
	                printf("\n");
                    return;
            }
        }while(!cont);
	}

	if(f != &stdin)
        fclose(f);
    else 
        fclose(console);
	
	printf("\n");
}
