#ifndef _PRINTF_H
#define _PRINTF_H

int printf(char *format, ...);
int sprintf(char *str, char *format, ...);
int vsprintf(char *str, char *format, int *args);
#endif
