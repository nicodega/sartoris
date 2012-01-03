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

char	outtext[256];
char	num[20];

enum { EXPAND, VERBATIM }; 

int di2a(int x, char *s);
int di2ax(unsigned int i, char *s);
int dsprintf_sprintf_strcp(char *str, char *buf);

int dvsprintf(char *str, char *format, int *args) {
	int a, i, j;
	char c;
	int state;	
	
	i=j=a=0;
	state = VERBATIM;
	while(c = *format++) {
		switch (state) {
		case VERBATIM:
			switch (c) {
			case '%':
				state = EXPAND;
				break;
			default:
				str[j++] = c;
				break;
			}
			break;
		case EXPAND:
			switch (c) {
			case '%':
				str[j++] = c;
				break;
			case 'd': case 'D': case 'i': case 'I':
				j += di2a(args[a++], &str[j]);
				break;
			case 'x': case 'X':
				j += di2ax(args[a++], &str[j]);
				break;
			case 's': case 'S':
				j += dsprintf_sprintf_strcp((char *) args[a++], &str[j]);
			}
			state = VERBATIM;
			break;
		}
	}
	
	str[j] = '\0';
	return 0; 
	
}

int di2a(int x, char *s) 
{
	char digits[11];
	int i=0, j=0;
	int retval;
    int neg = 0;

    if(x < 0)
    {
        neg = 1;
        x = -x;
    }

	do {
		digits[i++] = x % 10;
		x = x / 10;
	} while (x);
	
    if(neg)
    {
        digits[i] = '-';
        i++;
    }

	retval = i; 	
	
	while (i--) {
		s[j++] = '0' + digits[i];
	}
	
	return retval;
}

int di2ax(unsigned int x, char *s) {
	char digits[8];
	int i=0, j=0;
	int retval;
	char c;
	
	
	do {
		digits[i++] = x % 16;
		x = x / 16;
	} while (x);
	
	s[0] = '0';
	s[1] = 'x';
	
	retval = i+2;
		
	while (i--) {
	  c = digits[i];
	  if (c < 10) {
	    c += '0';
	  } else {
	    c = c - 10 + 'a';
	  }
	  s[(j++) + 2] = c;
	}
	
	return retval;
}

/* don't copy the \0 */

int dsprintf_sprintf_strcp(char *str, char *buf) {
	int i=0;
	char c;
	
	while( c=str[i] ) { buf[i++]=c; }
	
	return i;
}


void bochs_console_print(char *str)
{
    int i = 0;
    while(str[i] != '\0')
    {
        __asm__ __volatile__ ("outb %1, %0" : : "dN" (0xe9), "a" (str[i]));
        i++;
    }
}

int print(char *format, ...) 
{    
	/* call dvsprintf to construct string */
	dvsprintf(outtext, format, (int*) (&format + 1));

    bochs_console_print(outtext);
	return 0;
}

