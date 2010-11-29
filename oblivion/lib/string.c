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

#include <lib/structures/string.h>

int strreplace(char *str, int start, char sought, char rep)
{
	int i = 0, t = 0;
	while(str[i] != '\0')
	{
		if(str[i] == sought){ str[i] = rep; t++;} 
		i++;
	}
	return t;
}

int isnumeric(const char *str)
{
	while(*str != '\0')
	{
		if(*str < 0x30 || *str > 0x39) return 0; 
		str++;
	}
	return 1;
}

int atoi(const char *nptr)
{
	// on i386 long and int are 32 bit integers
	return (int)atol(nptr);	
}

long atol(const char *nptr)
{
	long ret = 0, pow = 1;
	int l = len(nptr) - 1;

	if(*nptr == '-')
	{
		pow = -1;
		l--;
	}

	while(l >= 0)
	{
		ret += (nptr[l] - 0x30) * pow;
		pow *= 10; l--;
	}

	return ret;
}

int len(const char* str)
{
	int i = 0;

	if(str == NULL) return 0;

	while(str[i] != '\0')
	{
		i++;
	}

	return i;
}

int streq(char* str1, char* str2)
{
	int i = 0;

	if(str1 == NULL && str2 == NULL) return -1;
	if(str1 == NULL || str2 == NULL) return 0;

	while(str1[i] != '\0' && str2[i] != '\0' && str1[i] == str2[i])
	{
		i++;
	}

	return str1[i] == str2[i];
}

int istreq(int start, int end, char* str1, char* str2)
{
	int i = start;

	if(str1 == NULL && str2 == NULL) return -1;
	if(str1 == NULL || str2 == NULL) return 0;

	while(i < end && str1[i] != '\0' && str2[i] != '\0' && str1[i] == str2[i])
	{
		i++;
	}

	return str1[i] == str2[i];
}

char* strcopy(char* str)
{
	int i = 0;

	char *c = (char*)malloc(len(str) + 1);

	while(str[i] != '\0')
	{
		c[i] = str[i];
		i++;
	}

	c[i] = '\0';

	return c;
}

char* substring(char* str, int start, int length)
{
	int i = start, j = 0;
	int end = start + length;
	char* ret = (char*)malloc(length + 1);

	if((start + length) > len(str)) return (char*)NULL;

	while(j < length)
	{
		ret[j] = str[i];
		i++;
		j++;
	}

	ret[j] = '\0';

	return ret;
}

int strprefix(char* str1, char* str2)
{
	int i = 0;

	if(str1 == NULL || str2 == NULL) return 0;

	while(str1[i] != '\0' && str2[i] != '\0' && str1[i] == str2[i])
	{
		i++;
	}

	return str1[i] == '\0';
}

char *strgetprefix(char* str1, char* str2, int start1, int start2)
{
	int i = start1;
	int j = start2;
	char* ret;

	if(str1 == NULL && str2 == NULL) return (char*)NULL;
	if(str1 == NULL || str2 == NULL) return (char*)NULL;

	while(str1[i] != '\0' && str2[j] != '\0' && str1[i] == str2[j])
	{
		i++;
		j++;
	}

	if(str1[i] == '\0')
	{
		ret = substring(str1,start1,i - start1);
	}
	else
	{
		ret = substring(str2,start2,j - start2);
	}

	return ret;
}

int istrprefix(int start,char* str1, char* str2)
{
	int i = start;
	int j = 0;

	if(start >= len(str2)) return 0;


	if(str1 == NULL && str2 == NULL) return -1;
	if(str1 == NULL || str2 == NULL) return -1;

	while(str1[j] != '\0' && str2[i] != '\0' && str1[j] == str2[i])
	{
		i++;
		j++;
	}

	return str1[j] == '\0';
}

void istrcopy(char* source, char* dest, int start)
{

	int i = 0;

	while(source[i] != '\0')
	{
		dest[start + i] = source[i];
		i++;
	}
	dest[start + i] = '\0';
}

int estrcopy(char* source, char* dest, int start, int length)
{
	int i = 0, k = 0;

	while(source[start + i] != '\0' && k < length)
	{
		dest[k] = source[start + i];
		i++; k++;
	}
	if(k < length)
	{
		dest[k] = '\0';
		k++;
	}

	return k;
}

int last_index_of(char *str, char c)
{
	int i = 0, k = -1;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == c) k = i;
		i++;	
	}
	return k;
}

int last_index_of_str(char *str, char *pat)
{
	int i = 0, k = -1;

	if(str == NULL || pat == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == pat[0] && strprefix(pat, str+i)) k = i;
		i++;	
	}
	return k;
}

int first_index_of(char *str, char c, int start)
{
	int i = start;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == c) break;
		i++;	
	}

	if(str[i] == '\0') return -1;	

	return i;
}

int first_index_of_str(char *str, char *pat, int start)
{
	int i = start;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == pat[0] && strprefix(pat, str+i)) break;
		i++;	
	}

	if(str[i] == '\0') return -1;	

	return i;
}

/* static string functions (same as in str.h and str.o)*/
int strlen(char* str){
	int i = 0;
	while(str[i]) i++;
	return i;
}

int strcmp(const char* a, const char* b){
	int i = 0;
	while(a[i]!='\0' && b[i]!='\0' && a[i]==b[i]) i++;
	return (a[i]-b[i]);
}

int strcp(char* dest, char* src){
	int i = 0;
	while(src[i]!='\0'){
		dest[i] = src[i];
		i++;
	}
	dest[i] ='\0';

	return i;
}

int strword(int word, char* str, int sSize, char* dest, int dSize){
	int i = 0;
	int j = 0;
	int w = 0;

	while (w < word){
		while(i < sSize && str[i] == ' ') i++;
		if (i == sSize || str[i] == '\0') return 0;
		
		j = 0;
		
		while (i < sSize && str[i] != ' ' && str[i] != '\0' && str[i] != '\n' && j < dSize - 1){
			if (w == word - 1) {
				dest[j] = str[i];
				j++;
			}
			i++;
			
		}
		if (w == word - 1) dest[j] = '\0';
		w++;
	}
 	return 1;
}

/*
 * Concatena src al final de dest
 */
char* strcat(char* dest, const char* src){
	int i = 0;
	int j = 0;
	while(dest[i]) i++;
	while(src[j]){
		dest[i] = src[j];
		i++; j++;
	}
	dest[i] ='\0';
	return dest;
}

/*
 * Elimina los espacios a la izquierda de un string
 */
char* ltrim(char* str){
	int c = 0;
	int i;
	while(str[c] && str[c] == ' ') c++;
	i = c;
	while(str[i]){
		str[i-c] = str[i];
		i++;
	}
	str[i-c] = '\0';
	return str;
}

/*
 * Elimina los espacios a la derecha de un string
 */
char* rtrim(char* str){
	int i;
	i = strlen(str) - 1;

	while(i >= 0 && str[i] == ' '){
		str[i] = '\0';
		i--;
	}
	return str;
}

char* trim(char* str){
	ltrim(str);
	return rtrim(str);
}

int prefix(char *p, char *s) {
  int i=0;
  while (p[i]) {
    if (!s[i] || p[i]!=s[i]) return 0;
    i++;
  }
  
  return 1;
}

char* substr(char* s, int init, unsigned int len, char* d){
	int i = 0;
	
	if (strlen(s) >= init){
		while(s[init]){
			d[i] = s[init];
			init++;
			i++;
		}
		d[i] = '\0';
	}
	return d;
}

// splits a string on the given separator.
// returns ammount of strings on dest
// if a " is found no splitting will be performed
// until a closing " is found
// unless ignore_blocks is 1
int strsplit(int start, char separator, int ignore_blocks, char* str, char* dest){
	int i = start, j = 0, ln = 0, count = 0, lastbrk = 0, brk = 0;

	trim(str);

	ln = len(str);

	if(ln > 0) count = 1;

	while(i < ln)
	{
		if(str[i] == '"' && !ignore_blocks)
		{
			brk = !brk;
		}
		else if(str[i] == separator && !brk && !lastbrk)
		{
			dest[j] = '\0'; // this will prepare it for get_parameters
			
			lastbrk = 1;
			j++;
			count++;
		}
		else if(str[i] != separator)
		{
			dest[j] = str[i];
			j++;
			lastbrk = 0;
		}
		i++;
	}

	return count;
}



/*
 * Elimina todos los caracteres a la derecha de un string hasta hallar el 
 * caracter tk. Si rmtk es distinto de 0, se remueve hasta el caracter tk
 * inclusive.
 */
char* strrmr(char* s, char tk, char rmtk){
	int i;
	i = strlen(s)-1;

	while(i>0 && s[i] != tk)
		i--;
	
	if(i<0){
		s[0] = '\0';
	} else {
		if(rmtk)
			s[i] = '\0';
		else
			s[i+1] = '\0';
	}
	return s;
}

/*
 * Elimina todos los caracteres a la izquierda de un string hasta hallar el 
 * caracter tk. Si rmtk es distinto de 0, se remueve hasta el caracter tk
 * inclusive.
 */
 char* strrml(char* s, char tk, char rmtk){
	int i;
	int j;
	int d;
	i = 0;

	while(s[i] && s[i] != tk)
		i++;
	
	if(s[i]){
		d = (rmtk)?1:0;
		j = 0;
		while(s[i+j+d]){
			s[j] = s[i+j+d];
			j++;
		}
		s[j]='\0';
	} else {
		s[0]='\0';
	}
	return s;
}

