/*  
*   Sartoris 0.5 i386 kernel screen driver
*   
*   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@gmail.com
*/

/* 
* used to print dumps, critical messages, etc
*/


/* this code uses the flat-mode descriptors only */

#include "sartoris/scr-print.h"

#define TEXT_ADDRESS 0xB8000    

extern void conf_vga(void);

int curr_pos = 0; // current cursor position

void k_scr_init()
{
    conf_vga();
    curr_pos = 0;
}

void k_scr_moveUp()
{
    int i = 0;
    char* buff;

    /* copy memory up */

    buff = (char*)TEXT_ADDRESS;  

    while (i < 4000)   /* process 24 lines */
    {
        *buff = *(buff + 160);
        buff++;
        i++;
    }

    curr_pos -= 80;

    return;
}

void k_scr_newLine()
{
    curr_pos = curr_pos + (80 - (curr_pos % 80));
    char* buff = (char*)(TEXT_ADDRESS + (curr_pos << 1));    /* advance 160 bytes and substract current possition */

    if ((long)buff >= TEXT_ADDRESS + 0xfa0) 
    { 
        k_scr_moveUp(); 
    }
    return;
}

void k_scr_print(char *text, unsigned char att)
{
    int i = 0;
    char* buff = (char*)(TEXT_ADDRESS + (curr_pos << 1));

    while (text[i] != '\0')
    { 
        if (text[i] == '\n') 
        {
            k_scr_newLine();
            buff = (char*)(TEXT_ADDRESS + (curr_pos << 1));
            i++;
        }
        else
        {
            *buff = text[i];
            buff++;

            *buff = att;
            buff++;

            curr_pos++;

            if (curr_pos >= (80*24))
            { 
                k_scr_moveUp(); 
            }

            i++;
        }
    }
}

void k_scr_hex(int n, unsigned char att) 
{
    int i;
    unsigned char c;
    unsigned char t[8];
    char* buff = (char*)(TEXT_ADDRESS + (curr_pos << 1));

    for(i=7; i>=0; i--) 
    {
        c = (unsigned char) (n & 0xf);
        if (c > 9) { c = c + 'a' - 0xa; } else { c = c + '0'; }
        t[i] = c;
        n = n >> 4;
    }

    for(i=0; i<8; i++) {
        *buff = t[i];
        buff++;
        *buff = att;
        buff++;
        curr_pos++;
    }

    if ((long)buff >= 0xB8fa0) { k_scr_moveUp(); }

}

void k_scr_clear()
{
    int i = 0;
    char *buff = (char*)TEXT_ADDRESS;

    while (i < 4000)    /* cambio todo */
    {
        *buff = ' ';
        buff++;
        *buff = 0;
        buff++;
        i = i + 2;
    }

    curr_pos = 0;

    return;
}

enum { EXPAND, VERBATIM };

int i2a(int x, char *s);
int i2ax(unsigned int i, char *s);
int sprintf_sprintf_strcp(char *str, char *buf);

int sprintf(char *str, char *format, ...) {
    return vsprintf(str, format, (int*) (&format + 1));
}

int vsprintf(char *str, char *format, int *args) {
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
                        j += i2a(args[a++], &str[j]);
                        break;
                    case 'x': case 'X':
                        j += i2ax(args[a++], &str[j]);
                        break;
                    case 's': case 'S':
                        j += sprintf_strcp((char *) args[a++], &str[j]);
                    }
                state = VERBATIM;
                break;
        }
    }

    str[j] = '\0';
    return 0; 

}

int i2a(int x, char *s) {
    char digits[10];
    int i=0, j=0;
    int retval;

    do {
        digits[i++] = x % 10;
        x = x / 10;
    } while (x);

    retval = i; 	

    while (i--) {
        s[j++] = '0' + digits[i];
    }

    return retval;
}

int i2ax(unsigned int x, char *s) 
{
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
int sprintf_strcp(char *str, char *buf) {
    int i=0;
    char c;

    while( c=str[i] ) { buf[i++]=c; }

    return i;
}

char buf[512];

int kprintf(unsigned char att, char *format, ...) 
{	/* call vsprintf to construct string */
    vsprintf(buf, format, (int*) (&format + 1));

    k_scr_print(buf, att);

    return 0;	
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

int bprintf(char *format, ...) 
{	/* call vsprintf to construct string */
    vsprintf(buf, format, (int*) (&format + 1));

    bochs_console_print(buf);

    return 0;	
}
