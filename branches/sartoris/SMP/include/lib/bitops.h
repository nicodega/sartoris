#ifndef _BITOPS_H_
#define _BITOPS_H_

/* these functions assume the params are valid */
#define UNIT (sizeof(unsigned int) * 8-1)

#define BPINT (sizeof(unsigned int) << 3)
#define BITMAP_SIZE(a) (((a) + (BPINT - ((a) % BPINT))) / BPINT)

int getbit(unsigned int *array, int pos);
void setbit(unsigned int *array, int pos, int value);
int getbit_off(unsigned int *array, int offset, int pos);

/*
Get the array position of the bit.
*/
extern inline int getbit_pos(int pos);

#endif
