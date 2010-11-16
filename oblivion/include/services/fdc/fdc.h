/*  
 *   Oblivion 0.1 floppy drive controller  service header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


//////////////////////////////////////
// Everything you change here should /
// be changed in fdc.s too           /
//////////////////////////////////////

// MESSAGES //

struct fdc_command {
  int op;
  int block;
  int smoid;
  int ret_port;
};

struct fdc_response {
  int op;
  int result;
  int res1;
  int res2;
};

// OP //

#define READ_BLOCK 0
#define WRITE_BLOCK 1
#define FDC_DIE -1			// kill the service (not implemented yet)
	
// RESULT //

#define FDC_OK 0
#define FDC_FAIL -1 
	
// PORT //

#define FDC_COMMAND_PORT 1 
#define FDC_DMA_TRANSACTIONS 2
