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

enum { EXPAND, VERBATIM }; 

#include <lib/sprintf.h>

int sprintf(char *str, char *format, ...);
int vsprintf(char *str, char *format, int *args);
int sprintf_strcp(char *str, char *buf, int len, int width, int flags);
int isnumericpf(char *str, int len);
long atoll(char *nptr, int len);

int sprintf(char *str, char *format, ...) {
	return vsprintf(str, format, (int*) (&format + 1));
}

#define MAX(a,b) ((a >= b)? a : b)
#define MIN(a,b) ((a <= b)? a : b)

int vsprintf(char *str, char *format, int *args) {
	int a, i, j;
	char c;
	int state;	
	int expand = 0;
    int param = -1;
    int precision = -1;
    int width = -1;
    int flags = 0;
    char digits[30];

	i=j=a=0;
	state = VERBATIM;
	while(format[i]) 
    {
        c = format[i];
		switch (state) 
        {
		    case VERBATIM:
			    switch (c) 
                {
			        case '%':
				        state = EXPAND;
                        expand = i;
				        break;
			        default:
				        str[j++] = c;
				        break;
			    }
			    break;
		    case EXPAND:
                // read modifiers
                switch (c) 
                {
                    /*
                    - if it has a $, it has a parameter
                    - If it has a +, -, # or 0 (or a number after % or the $ sign) it has flags
                    - If it has a . then it specified precision
                    */
                    case '$':
                        // behind us is the parameter
                        if(flags > 0 || param != -1) return -1;
                        if(!isnumericpf(&format[expand+1], i-expand-1))
                            return -1;
                        param = atoll(&format[expand+1], i-expand-1);
                        expand = i;
                        break;
                    case '+':
                        flags |= PRINTF_FLAG_PLUS;
                        expand++;
                        break;
                    case '-':
                        flags |= PRINTF_FLAG_MINUS;
                        expand++;
                        break;
                    case '*':
                        // I won't support this one yet
                        expand++;
                        break;
                    case '#':
                        flags |= PRINTF_FLAG_NUM;
                        expand++;
                        break;
                    case '0':
                        if(precision == -1 && !isnumericpf(&format[expand+1], i-expand-1))
                        {
                            flags |= PRINTF_FLAG_0;
                            expand++;
                        }
                        break;
                    case '.':
                        // did we have width defined before?
                        if(isnumericpf(&format[expand+1], i-expand-1))
                        {
                            width = atoll(&format[expand+1], i-expand-1);
                            expand = i;
                        }
                        else
                        {   
                            expand++;
                        }
                        precision = i;
                        break;
                    // length
                    case 'h':
                        if(flags > 8) return -1;
                        if(flags & PRINTF_FLAG_PROMOTED_CHAR)
                        {
                            flags &= ~PRINTF_FLAG_PROMOTED_CHAR;
                            flags |= PRINTF_FLAG_PROMOTED_SHORT;
                        }
                        else
                        {
                            flags |= PRINTF_FLAG_PROMOTED_CHAR;
                        }
                        break;
                    case 'l':
                        if(flags > 8) return -1;
                        if(flags & PRINTF_FLAG_PROMOTED_LONG)
                        {
                            flags &= ~PRINTF_FLAG_PROMOTED_LONG;
                            flags |= PRINTF_FLAG_PROMOTED_LL;
                        }
                        else
                        {
                            flags |= PRINTF_FLAG_PROMOTED_LL;
                        }
                        break;
                    case 'L':
                        if(flags > 8) return -1;
                        flags |= PRINTF_FLAG_PROMOTED_LD;
                        break;
                    case 'z':
                        if(flags > 8) return -1;
                        flags |= PRINTF_FLAG_PROMOTED_SIZET;
                        break;
                    case 'j':
                        if(flags > 8) return -1;
                        flags |= PRINTF_FLAG_PROMOTED_INTMAX;
                        break;
                    case 't':
                        if(flags > 8) return -1;
                        flags |= PRINTF_FLAG_PROMOTED_PTRDIFF;
                        break;
                    // types
			        case '%':
				        str[j++] = c;
                        state = VERBATIM;
				        break;
			        case 'd': case 'D': case 'i': case 'I':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision != -1)
                                width = atoll(&format[expand+1], i-expand-1);
                        }
                        j += padding(&str[j], digits, i2a(args[(param == -1)? a++ : param], digits, flags), flags, width, PADDING_DEC);
                        state = VERBATIM;
				        break;
                    case 'u': case 'U':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision != -1)
                                width = atoll(&format[expand+1], i-expand-1);
                        }
				        j += padding(&str[j], digits, i2u(args[(param == -1)? a++ : param], digits, flags), flags, width, PADDING_DEC);
                        state = VERBATIM;
				        break;
			        case 'x': case 'X': case 'p': case 'P':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision != -1)
                                width = atoll(&format[expand+1], i-expand-1);
                        }
				        j += padding(&str[j], digits, i2ax(args[(param == -1)? a++ : param], digits, flags, (c == 'X')), flags, width, PADDING_HEX);
                        state = VERBATIM;
                        break;
                    case 'o': case 'O':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision != -1)
                                width = atoll(&format[expand+1], i-expand-1);
                        }
				        j += padding(&str[j], digits, i2ao(args[(param == -1)? a++ : param], digits, flags), flags, width, PADDING_OCT);
                        state = VERBATIM;
                        break;
                    case 'f': case 'F':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision)
                                precision = atoll(&format[expand+1], i-expand-1); // it's the precision
                            else
                                width = atoll(&format[expand+1], i-expand-1);
                        }
                        if(precision > 10) return -1; 
				        j += padding(&str[j], digits, i2f(*((double*)&args[(param == -1)? a++ : param]), digits, flags, precision, (c == 'F')), flags, width, PADDING_DEC);
                        a++; // increment a again, because it's a 64bit double.
                        state = VERBATIM;
				        break;
                    case 'c': case 'C':
                        if(flags != 0 || width != -1 || precision != 0) return -1;
                        str[j] = args[(param == -1)? a++ : param];
				        j += 1;
                        state = VERBATIM;
				        break;
			        case 's': case 'S':
                        if(expand+1 != i)
                        {
                            if(!isnumericpf(&format[expand+1], i-expand-1))
                                return -1;
                            if(precision)
                                precision = atoll(&format[expand+1], i-expand-1); // it's the precision
                            else
                                width = atoll(&format[expand+1], i-expand-1);
                        }
                        j += sprintf_strcp((char *) args[(param == -1)? a++ : param], &str[j], precision, width, flags);                        
			            state = VERBATIM;
                        break;
                    case ' ': case '\0':
                        return -1;
			    }
                if(state == VERBATIM)
                {
                    param = -1;
                    precision = -1;
                    width = -1;
                    flags = 0;
                }
			    break;
		}
        i++;
	}
	
	str[j] = '\0';
	return j; 
	
}

long atoll(char *nptr, int len)
{
    long ret = 0; 
    int i = 0;
    while (i < len && (*nptr >= '0') && (*nptr <= '9')) 
    { 
        ret *= 10; 
        ret += *nptr - '0'; 
        nptr++; 
        i++; 
    } 
    
	return ret;
}

int isnumericpf(char *str, int len)
{
    if(!len) return 0;
    int i = 0;
	while(i < len)
	{
		if(*str < 0x30 || *str > 0x39) return 0; 
		str++;
        i++;
	}
	return 1;
}

int padding(char *str, char *digits, int digits_len, int flags, int width, int type)
{
    int j = 0, i = digits_len, ret;
    if(width == -1 || digits_len >= width)
    {
        while (i--) 
        {
		    str[j++] = digits[i];
	    }
        return digits_len;
    }
    else
    {
        char pad = ' ';
        ret = width;
    
        if(flags & PRINTF_FLAG_0)
        {
            pad = '0';
        
            if(!(flags & PRINTF_FLAG_MINUS))
            {
                // if PRINTF_FLAG_PLUS or PRINTF_FLAG_NUM is present or it has a negative sign
                // and PRINTF_FLAG_0 is set, we will write the "sign" first, then the padding.
                if(type == PADDING_DEC && (flags & PRINTF_FLAG_PLUS))
                {
                    str[j++] = digits[digits_len-1];
                    width++;
                    i--;
                }
                else if (type == PADDING_HEX && (flags & PRINTF_FLAG_NUM))
                {
                    str[j++] = digits[digits_len-1];
                    str[j++] = digits[digits_len-2];
                    width += 2;
                    i -= 2;
                }
                else if (type == PADDING_OCT && (flags & PRINTF_FLAG_NUM))
                {
                    str[j++] = digits[digits_len-1];
                    width++;
                    i--;
                }
            }
        }

        // add padding
        if(!(flags & PRINTF_FLAG_MINUS))
        {
            width -= digits_len;
            while(j < width)
            {
                str[j++] = pad;
            }
        }

        while (i--) 
        {
		    str[j++] = digits[i];
	    }

        if(flags & PRINTF_FLAG_MINUS)
        {
            while(j < width)
            {
                str[j++] = pad;
            }
        }

        return ret;
    }
}

int i2u(unsigned int x, char *digits, int flags) 
{
	int i=0;
	
	do 
    {
		digits[i++] = '0' + x % 10;
		x = x / 10;
	} while (x);
	
    if(flags & PRINTF_FLAG_PLUS)
        digits[i++] = '+';

	return i;
}

int i2a(int x, char *digits, int flags) 
{
    int ret;
    if(x < 0)
    {
        if(flags & PRINTF_FLAG_PLUS)
        {
            ret = i2u(-x, digits, flags);
            digits[ret-1] = '-';
            return ret;
        }
        else
        {
            ret = i2u(-x, digits, flags);
            digits[ret] = '-';
            return ret + 1;
        }
    }
    else
    {
        return i2u(x, digits, flags);
    }
}

int i2ax(unsigned int x, char *digits, int flags, int upper) 
{
	int i=0;
	char c;

    do 
    {
        c = (x & 0xF);
        if (c < 10) 
            c += '0';
        else
            c = (c - 10) + 'A';
        digits[i++] = c;
        x = (x >> 4);
    } while (x);

    if(flags & PRINTF_FLAG_NUM)
    {
        if(upper)
            digits[i++] = 'X';
        else
            digits[i++] = 'x';
        digits[i++] = '0';
    }
	
	return i;
}

int i2ao(unsigned int x, char *digits, int flags) 
{
	int i=0;
	char c;

    do 
    {
        c = (x & 0x7);
        if (c < 10) 
            c += '0';
        digits[i++] = c;
        x = (x >> 3);
    } while (x);

    if(flags & PRINTF_FLAG_NUM)
        digits[i++] = '0';
    
	return i;
}

double pow10[] = { 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0, 10000000000.0 };

int i2f(double x, char *digits, int flags, int precision, int upper)
{
    // NOTE: this is a very very very bad implementation of this...
    // but it's simpler, until I've got math.h implemented
    double zero = 0.0;
    double inf = 1.0 / zero;
    
    if(x == inf)
    {
        // infinite
        if(upper)
            sprintf_strcp(digits, "INF", -1, -1, 0);
        else
            sprintf_strcp(digits, "inf", -1, -1, 0);
        return 3;
    }
    else if(x != x)
    {
        // NaN
        if(upper)
            sprintf_strcp(digits, "NAN", -1, -1, 0);
        else
            sprintf_strcp(digits, "nan", -1, -1, 0);
        return 3;
    }
    else
    {
        if(precision == -1)
            precision = 6;

        /*1234 -> 4321
        34.500 -> 005.43

        34 -> 
        */
        // get the integer part (use a 64 bit integer, for it's a double precision number)
        int neg = (x < 0);
        if(neg) x = -x;
        long long intp = (long long)x;
        long long intp2 = intp;
        int i=0, j = 0;

        // get the fractional part up to precision characters
        x -= (double)intp;
        x = x * pow10[precision-1];
        intp = (long long)x;

        // print integer part, starting from precision
        i = precision;

        if(precision > 0)
        {
            digits[i++] = '.';
            j++;
        }

	    do 
        {
		    digits[i++] = '0' + intp2 % 10;
		    intp2 = intp2 / 10;
            j++;
	    } while (intp2);

        // write decimal values
        if(precision > 0)
        {
            i = 0;
            do 
            {
		        digits[i++] = '0' + intp % 10;
                intp = intp / 10;
                j++;
	        } while (i < precision);
        }
	
        if(flags & PRINTF_FLAG_PLUS)
        {
            if(neg)
                digits[j++] = '-';
            else
                digits[j++] = '+';
        }
        else if(neg)
        {
            digits[j++] = '-';
        }
                
        return j;
    }
}

/* don't copy the \0 */
int sprintf_strcp(char *str, char *buf, int len, int width, int flags)
{
	int i=0, slen = 0;
	char c;
    
    while( str[slen] ) { slen++; }

    if(len != -1)
        len = MIN(len, slen);
    else
        len = slen;

    // add padding
    if(width != -1 && !(flags & PRINTF_FLAG_MINUS))
    {
        width -= len;
        while(i < width)
            buf[i++] = ' ';
        len += width;
        while( str[i-width] && i < len ) 
        { 
            buf[i]=str[i-width];
            i++;
        }
    }
    else
    {
        while( str[i] && i < len ) 
        { 
            buf[i]=str[i];
            i++;
        }
    }
    
    if(width != -1 && (flags & PRINTF_FLAG_MINUS))
    {
        while(i < width)
            buf[i++] = ' ';
    }
	
	return len;
}
