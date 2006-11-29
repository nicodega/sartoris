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

#include <lib/iolib.h>

int main(int argc, char **argv) 
{
	int i = 0;
	char buff[50];

	if(argc > 1 && streq(argv[1], "-nostdin"))
	{
		printf("jedi knight line editor version 0.01 [nostdin]\n");
	}
	else
	{
		char *str = fgets(buff, 50, &stdin);
		if(str != NULL)
		{
			int ln = len(str);
			str[ln] = '\0';
			str[ln+1] = '\n';
			printf(str);
		}
		else
		{
			printf("gets returned NULL %i\n", argc);
		}
	}

	return 0;
} 
	
