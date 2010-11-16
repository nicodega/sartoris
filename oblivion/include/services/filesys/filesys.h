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

#define FS_MOUNT  0
#define FS_UMOUNT 1
#define FS_FORMAT 2
#define FS_READ   3
#define FS_WRITE  4
#define FS_CREATE 5
#define FS_DELETE 6
#define FS_GET_NAME 7 

#define FS_OK 0
#define FS_NO_SUCH_FILE -1
#define FS_FAIL -2

#define FS_CMD_PORT 1
#define FS_RESPONSE_PORT 2
#define BLOCK_SIZE 0x1000   // 4k

struct fs_command {
  unsigned char op;    // operation code
  unsigned char id;    // transaction ID (returns as it arrives)
  unsigned char index;  // used on FS_GET_NAME op
  unsigned char padding;// unused
  int smo_name;     // SMO with filename
  int smo_buff;     // SMO with destination or source buffer
  int ret_port;     // the desired response port
};

struct fs_response {
  unsigned char op;    // operation code
  unsigned char id;    // transaction ID (returns as it arrived)
  unsigned char padding[2];  // unused
  int result;       // operation result
  int smo_name;
  int smo_buff; 
};

struct file{
  char name[12];    // the last one must be \0
  unsigned int first_sector; // files are sequentially stored in the disk
};

#endif
