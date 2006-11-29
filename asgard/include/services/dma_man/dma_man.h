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

#ifndef DMA_SERV
#define DMA_SERV

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#define DMA_COMMAND_PORT 1

#define DMAMAN_UIID	0x00000006
#define DMAMAN_VER	1

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
  char thr_id;
  char msg2;
  unsigned int buff_size;          
} PACKED_ATT;

struct dma_response{
  int op;
  int result;
  int res1;
  short res2;
  char padding;
  char thr_id;
} PACKED_ATT;

#endif



