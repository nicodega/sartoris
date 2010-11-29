/*  
 *   Oblivion 0.1 floppy filesystem header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */



#ifndef FILESYS
#define FILESYS

////////////////////////////////
//    File System Messages    //
////////////////////////////////


#define FS_READ   1
#define FS_SIZE   2

#define FS_OK 0
#define FS_NO_SUCH_FILE -1
#define FS_FAIL -2

#define FS_CMD_PORT 1
#define FS_RESPONSE_PORT 2
#define BLOCK_SIZE 0x1000   // 4k

struct fs_command {
  unsigned char op;    // operation code
  unsigned char id;    // transaction ID (returns as it arrives)
  short offset;        // file offset to read
  short count;         // unused
  int smo_name;        // SMO with filename
  int smo_buff;        // SMO with destination or source buffer
  short ret_port;      // the desired response port
} __attribute__ ((__packed__));

struct fs_response {
  unsigned char op;    // operation code
  unsigned char id;    // transaction ID (returns as it arrived)
  unsigned char padding[2];  // unused
  int result;       // operation result
  int smo_name;
  int smo_buff; 
} __attribute__ ((__packed__));

#endif
