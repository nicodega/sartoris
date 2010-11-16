#ifndef DMA
#define DMA

void init_dma(int control);
void set_dma_channel(int channel, int physaddr, int length, int mode);


/* transfer types */

#define DMA_ENABLE       0
#define DMA_DISABLE      4      
#define ROTATING_PRI     16
#define MEM_TO_MEM_ALLOW 1

#endif
