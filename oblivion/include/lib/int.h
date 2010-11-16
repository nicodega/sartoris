/*  
 *   Oblivion 0.1 interrupt support header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */



#ifndef _INT_H_
#define _INT_H_

void enter_block(void);
void exit_block(void);
void ack_int_master(void);

#endif
