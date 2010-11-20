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

#include "drivers/screen/screen.h"

int curr_pos = 0; // current cursor position

void scr_clear()
{
    clear_screen();
    curr_pos = 0;
}

void do_print(char *str, int att) 
{
    char buf[81];
    int i, j, k;
    int rem;

    i = 0;
    while (str[i]!=0) 
    {
        j=0;
        rem = 80 - ( ((curr_pos>>1)) % 80 );
        while(j<rem && str[i]!=0) 
        {
            if (str[i]=='\n') 
            {
                i++;
                while(j<rem) { buf[j++]=' '; }
            } 
            else if (str[i]=='\t') 
            {
                i++;
                k=0;
                while(j<rem && k<4) 
                {
                    buf[j++]=' ';
                    k++;
                }
            }
            else 
            {
                buf[j++]=str[i++];
            }
        }

        buf[j]=0;
        string_print(buf, curr_pos, att);
        curr_pos += j*2;

        if(curr_pos>=80*25*2) 
        {
            curr_pos=80*24*2;
            scroll_up_one();            
        }
    }
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

int printf(unsigned char att, char *format, ...) 
{	/* call vsprintf to construct string */
    vsprintf(buf, format, (int*) (&format + 1));

    do_print(buf, att);

    return 0;	
}

