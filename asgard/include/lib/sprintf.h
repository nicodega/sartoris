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

#ifndef SPRINTFH
#define SPRINTFH

int i2a(int x, char *s);
int i2ax(unsigned int i, char *s);
int strcp(char *str, char *buf);
int sprintf(char *str, char *format, ...);
int vsprintf(char *str, char *format, int *args);

#endif

