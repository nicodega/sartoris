#ifndef DMA_SERV
#define DMA_SERV

#define DMA_COMMAND_PORT 0

/* DMA channel modes */

#define SINGLE_TRANSFER   64
#define BLOCK_TRANSFER    128
#define DEMAND_TRANSFER   0
#define AUTO_INIT         16
#define READ              8
#define WRITE             4
#define VERIFY            0
#define ADDRESS_DECREMENT 32

/* messages */

#define GET_CHANNEL   1
#define SET_CHANNEL   2
#define FREE_CHANNEL  4
#define DMA_DIE      -1

/* responses */

#define DMA_OK      0
#define DMA_FAIL   -1
#define ALLOC_FAIL  1
#define CHANNEL_TAKEN 2

struct dma_command{
  int op;           // operation
  int ret_port;
  char channel;
  char channel_mode;      
  char msg1;
  char msg2;
  unsigned int buff_size;          
};

struct dma_response{
  int op;
  int result;
  int res1;
  int res2;
};

#endif



