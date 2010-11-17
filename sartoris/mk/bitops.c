/*  
 *   Sartoris bitmap operations
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */


#include "lib/bitops.h"

/* these functions assume the params are valid */
#define UNIT (sizeof(unsigned int) * 8-1)

int getbit(unsigned int *array, int pos) 
{
    return (array[pos / UNIT] >> (UNIT - pos % UNIT)) & 1;
}

void setbit(unsigned int *array, int pos, int value) 
{
    int newpos = UNIT - (pos % UNIT);
    int apos = pos / UNIT;
    
    array[apos] = (array[apos] & (~(1 << newpos))) | value << newpos;
}

