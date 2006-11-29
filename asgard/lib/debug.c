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

#include "lib/debug.h"

int printRow=0;
int printCol=0;
int printLColor = 7;
char	outtext[80];
char	num[20];
int rows = 24;
int cols = 160;
	
void print (char *texto, unsigned int valor)
{
	char	aux;
	int		i, j;
	
	// prepare the number string
	i = 0;
	if (valor == 0)
	{
		num[0] = 0x30;
		i++;
	}
	else
	{
		while (valor != 0)
		{
			num[i] = (char) (valor % 10) + 0x30;
			valor = valor / 10;
			i++;
		}
	}

	num[i] = '\0';

	i--;
	j = 0;
	while (j < i)
	{
		aux = num[j];
		num[j] = num[i];
		num[i] = aux;
		i--;
		j++;
	}


	// prepare the output string

	i = 0;
	j = 0;
	while (texto[i] != '\0')
	{
		outtext[j] = texto[i];
		i++;
		j++;
	}

	outtext[j]=':';
	outtext[j+1]=' ';

	j = j + 2;
	i = 0;
	while (num[i] != '\0')
	{
		outtext[j] = num[i];
		i++;
		j++;
	}
	outtext[j] = '\0';


	if ( texto[0] == '#' )
	{
		string_print (outtext, printCol + printRow * cols, printLColor++);
		printRow = (printRow + 1) % rows;
	}
	else if ( texto[0] == '!' )
	{
		string_print (outtext, printCol + printRow * cols, printLColor++);
	}
	else
	{
		string_print (outtext, printCol + printRow * cols, 10);
		printRow = (printRow + 1) % rows;
	}
}


void clrscr(void)
{
	int i;
	for (i = 0; i<(24*80); i++) {
		string_print (" ", 2*i, 7);
	}

	printRow=0;
	printCol=0;
}
