#ifndef _BITOPS_H_
#define _BITOPS_H_

#define BPINT (sizeof(unsigned int) << 3)
#define BITMAP_SIZE(a) (((a) + (BPINT - ((a) % BPINT))) / BPINT)

int getbit(unsigned int *array, int pos);
void setbit(unsigned int *array, int pos, int value);

#endif
