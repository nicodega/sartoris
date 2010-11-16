/*  
 *   Oblivion 0.1 console service header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */



#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#define CSL_ACK_PORT 0
#define CSL_SGN_PORT 1

#define CSL_RESET 0
#define CSL_SWITCH 1
#define CSL_READ 2
#define CSL_WRITE 3

#define MAX_CSL_WRITE 512

struct csl_ctl_msg {
  int index;
  int command;
  int padding[2];
};

struct csl_io_msg {
  int command;
  int smo;
  int len;
  char delimiter;
  char attribute;
  char echo;
  char response_code;
};

struct csl_signal_msg {
  int term;
  char key;
  char alt;
  char padding[10];
};

struct csl_response_msg {
  int smo;
  char code;
  char padding[11];
};

#endif
