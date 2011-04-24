/*
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*     modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#ifndef SPRINTFH
#define SPRINTFH

#ifdef __cplusplus
extern "C" {
#endif

    
#define PRINTF_FLAG_PLUS             1
#define PRINTF_FLAG_MINUS            2
#define PRINTF_FLAG_0                4
#define PRINTF_FLAG_NUM              8
#define PRINTF_FLAG_PROMOTED_CHAR    16
#define PRINTF_FLAG_PROMOTED_SHORT   32
#define PRINTF_FLAG_PROMOTED_LONG    64
#define PRINTF_FLAG_PROMOTED_LL      128
#define PRINTF_FLAG_PROMOTED_LD      256
#define PRINTF_FLAG_PROMOTED_SIZET   512
#define PRINTF_FLAG_PROMOTED_INTMAX  1024
#define PRINTF_FLAG_PROMOTED_PTRDIFF 2048

#define PADDING_DEC 0
#define PADDING_HEX 1
#define PADDING_OCT 2

int padding(char *str, char *digits, int digits_len, int flags, int width, int type);
int i2u(unsigned int x, char *digits, int flags);
int i2a(int x, char *digits, int flags);
int i2ax(unsigned int x, char *digits, int flags, int upper);
int i2ao(unsigned int x, char *digits, int flags);
int i2f(double x, char *digits, int flags, int precision, int upper);
int sprintf(char *str, char *format, ...);
int vsprintf(char *str, char *format, int *args);

#ifdef __cplusplus
}
#endif

#endif

