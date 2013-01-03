/*  
 *   Sartoris bitmap operations
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */


#include "lib/bitops.h"

/*
Get the bit at position "pos" on the bitmap.
*/
int getbit(unsigned int *array, int pos) 
{
    return (array[pos / UNIT] >> (UNIT - pos % UNIT)) & 1;
}

/*
Set the bit at position "pos" on the bitmap.
*/
void setbit(unsigned int *array, int pos, int value) 
{
    int newpos = UNIT - (pos % UNIT);
    int apos = pos / UNIT;
    
    array[apos] = (array[apos] & (~(1 << newpos))) | value << newpos;
}

/*
Set the value of the bit at "pos" but considering 
we only have the array starting at offset available.
*/
int getbit_off(unsigned int *array, int offset, int pos) 
{
    return (array[pos / UNIT - offset] >> (UNIT - pos % UNIT)) & 1;
}

inline int getbit_pos(int pos) 
{
    return pos / UNIT;
}
