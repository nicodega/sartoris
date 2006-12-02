/*  
 *   Sartoris bitmap operations
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@cvtci.com.ar      
 */


#include "lib/bitops.h"

/* these functions assume the params are valid */

int getbit(unsigned int *array, int pos) {
    
    return (array[pos / (sizeof(unsigned int) * 8)] >>
	    (pos % (sizeof(unsigned int) * 8))) & 1;
}

void setbit(unsigned int *array, int pos, int value) {
    int newpos = (pos % (sizeof(unsigned int) * 8));
    int apos = pos / (sizeof(unsigned int) * 8);
    
    array[apos] = (array[apos] & (~(1 << newpos))) | value << newpos;
}
