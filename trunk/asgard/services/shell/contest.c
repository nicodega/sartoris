/*
*	Shell Service.
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
*
*	This command is only ment to test char devices on ofs service.
*
*/


#include "shell_iternals.h"


char *tty[NUM_TERMS] = {"/dev/tty0",
			"/dev/tty1",
			"/dev/tty2",
			"/dev/tty3",
			"/dev/tty4",
			"/dev/tty5",
			"/dev/tty6",
			"/dev/tty7"};

void contest(int term, char **args, int argc)
{
	// open current console
	FILE *f = fopen(tty[term], "w");
	char str[128] = {'\0',};
	char *cc = "\nCharacter typed was:  \n";

	if(f == NULL)
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	if(fputs("fputs is ok", f) < 0)
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}

	if(fputc('c', f) != 'c')
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}
	term_color_print(term, "\n", 11);

	if(fgets(str, 128, f) != str)
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}
	term_color_print(term, "\nInput was:", 11);
	term_color_print(term, str, 11);

	char cr = (char)fgetc(f);

	if(cr == EOF)
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 11);
		return;
	}
	cc[len(cc) - 2] = cr;
	term_color_print(term, cc, 11);

	fclose(f);

	return;
}
