#ifndef _BITOPS_H_
#define _BITOPS_H_

#define BITMAP_SIZE(a) (a / (sizeof(unsigned int) << 3))

int getbit(unsigned int *array, int pos);
void setbit(unsigned int *array, int pos, int value);

#endif
