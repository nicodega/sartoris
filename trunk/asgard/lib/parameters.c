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

int get_param_count(char *cn)
{
	int i = 0, ln = 0, count = 0, lastsp = 0, brk = 0, hadbrk = 0;

	trim(cn);

	ln = len(cn);

	if(ln > 0) count = 1;

	while(i < ln)
	{
		if(cn[i] == '"')
		{
			if(brk) hadbrk = 1;
			brk = !brk;
		}
		else if(cn[i] == ' ' && !brk && !lastsp)
		{
			if(hadbrk)
				cn[i-1] = '\0'; // this will prepare it for get_parameters
			else
				cn[i] = '\0'; // this will prepare it for get_parameters

			lastsp = 1;
			hadbrk = 0;
			count++;
		}
		else if(cn[i] != ' ')
		{
			lastsp = 0;
		}
		i++;
	}

	return count;
}

void get_parameters(char *params, int argc, char **args)
{
	int s = 0, count = 0;

	while(count < argc)
	{	
		if(params[s] == '"')
		{
			args[count] = &params[s+1];	
		}
		else
		{
			args[count] = &params[s];	
		}

		count++;

		if(count == argc) break;

		s += len(&params[s])+1; // find next param

		while(params[s] == ' ') s++;
	}
}
