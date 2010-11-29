/*
*   	STRING FUNCTIONS
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

#ifndef OFSSTRINGH
#define OFSSTRINGH

#include <lib/malloc.h>
#include <lib/const.h>

int len(const char* str);
int streq(char* str1, char* str2);
int istreq(int start, int end, char* str1, char* str2);
char* strcopy(char* str);
void istrcopy(char* source, char* dest, int start);
int estrcopy(char* source, char* dest, int start, int length);
int strprefix(char* str1, char* str2);
char *strgetprefix(char* str1, char* str2, int start1, int start2);
int istrprefix(int start,char* str1, char* str2);
char* substring(char* str, int start, int end);
int last_index_of(char *str, char c);
int first_index_of(char *str, char c, int start);
int last_index_of_str(char *str, char *pat);
int first_index_of_str(char *str, char *pat, int start);

/* following functions do not use malloc or m_malloc */
int strreplace(char *str, int start, char sought, char rep);
int atoi(const char *nptr);
long atol(const char *nptr);
int isnumeric(const char *str);
int dstrlen(char*);
int strcmp(const char*, const char*);
int strcp(char*, char*);
int strword(int word, char* str, int sSize, char* dest, int dSize);
int prefix(char *p, char *s);
char* strcat(char* dest, const char* src);
char* ltrim(char* str);
char* rtrim(char* str);
char* trim(char* str);
char* substr(char* s, int init, unsigned int len, char* d);
int strsplit(int start, char separator, int ignore_blocks, char* str, char* dest);
char* strrmr(char* s, char tk, char rmtk);
char* strrml(char* s, char tk, char rmtk);

#endif

