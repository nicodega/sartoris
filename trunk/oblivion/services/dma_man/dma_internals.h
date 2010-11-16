#ifndef DMA_INT
#define DMA_INT

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <services/dma_man/dma_man.h>
#include <drivers/dma/dma.h>
#include <oblivion/layout.h>
#include <lib/malloc.h>

#define BUFF_SIZE 10*512

struct channel {
  int task;
  int *buffer;
  int buff_size;
  int smo;
};



#endif

