/*  
 *   Oblivion ram filesystem header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#ifndef _RAMFS_H_
#define _RAMFS_H_

#define RAMFS_READ 0

#define READ_OK 0
#define NO_SUCH_FILE -1
#define SMO_ERROR -2

#define FILENAME_SIZE 12
#define FILE_SIZE 0x800

struct ramfs_io_msg {
  int command;
  int filename_smo;
  int buffer_smo;
  char response_code;
  char response_port;
  char padding[2];
};

struct ramfs_response_msg {
  int response_code;
  int status;
  int buffer_smo;
  int filename_smo;
};

#endif
