/*  
 *   Oblivion 0.1 floppy filesystem service
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */


#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <services/fdc/fdc.h>
#include <oblivion/layout.h>
#include <services/filesys/filesys.h> 
#include <drivers/screen/screen.h> 

//////////////////////////////////
// This service implements a    //
// poor floppy filesystem       //
//////////////////////////////////

struct file file_t[32];   // disk first sector
int mounted = 0;
unsigned char buffer[512]; // buffer used to put the read sector 
int buff_smoid, file_t_smoid;


int msgproc(struct fs_command *, int id);
void init();
int format();
void create_file_sys();
int create_entry(int);
int delete_entry(int);
int resolve(int smo_name);
int streq(char *s, char *t);
int read(unsigned char sector, int smoid);
int write(unsigned char sector, int smoid);
int get_name(int index, int smo_name);

void run(void){

  struct fs_command msg;
  struct fs_response result;
  int id,temp = 0;  
  
  __asm__ ("sti" : :);
  
  init();

  while(1){
  
    while(get_msg_count(FS_CMD_PORT) == FS_OK){
	run_thread(SCHED_THR);
    }    

    // process the message 
    get_msg(FS_CMD_PORT,&msg, &id);
    result.result = msgproc(&msg,id);
    
    // generate response message
    result.id = msg.id;
    result.op = msg.op;
    result.smo_name = msg.smo_name;
    result.smo_buff = msg.smo_buff;

    // send it back
    send_msg(id, msg.ret_port, &result);
  }
}

void init(){

  // create the buffer and file_t smoid and change owner
  // to FDC

  buff_smoid = share_mem(FDC_TASK, &buffer, 128, WRITE_PERM | READ_PERM);
  file_t_smoid = share_mem(FDC_TASK, &file_t, 128, READ_PERM | WRITE_PERM);

  open_port(FS_CMD_PORT, 0, UNRESTRICTED);
}

int msgproc(struct fs_command *msg,int id){

  int beg,i;
  switch (msg->op){
  case FS_MOUNT:
    // load the first sector with file description
    if (!mounted){
      if(read(0x0,file_t_smoid) == FS_OK) mounted = 1;
       
      else{
	mounted = 0;
	return FS_FAIL;}
    }
    break;
  case FS_UMOUNT:
      mounted = 0;
    break;
  case FS_FORMAT:
    // create a file system with no files and just the / directory

    if(format() == FS_OK){ // this should perform a hardware format to the floppy
      create_file_sys();

      if(write(0x0, file_t_smoid) != FS_OK){
	mounted = 0;
	return FS_FAIL;
      }
    }else{return FS_FAIL;}

    mounted = 1; // just in case...

    break;
  case FS_READ:
    if(!mounted){return FS_FAIL;}

    beg = resolve(msg->smo_name);
    
    if(beg == -1){return FS_NO_SUCH_FILE;}  // file not found

    // read a LOGICAL BLOCK

    i = 0;

    while(i < (BLOCK_SIZE / 512)){
      read(beg+i, buff_smoid);
      write_mem(msg->smo_buff, 128*i, 128, buffer);
      i++;
    }

    break;
  case FS_WRITE:
    if(!mounted){return FS_FAIL;}

    beg = resolve(msg->smo_name);
    
    if(beg == -1){return FS_NO_SUCH_FILE;}

    // write a LOGICAL BLOCK

    i = 0;

    while(i < (BLOCK_SIZE / 512)){
      read_mem(msg->smo_buff, 128*i, 128, buffer);
      if(write(beg+i, buff_smoid) == FS_FAIL){return FS_FAIL;};
      i++;
    }
  
    break;
  case FS_CREATE:
    if(!mounted){return FS_FAIL;}
    // create an entry for a file in the file_t

    if(create_entry(msg->smo_name) == FS_OK){
      if(write(0x0, file_t_smoid) != FS_OK){   // update file table
	return FS_FAIL;
      }
    }else{
      return FS_FAIL;
    }

    break;

  case FS_DELETE:
    if(!mounted){return FS_FAIL;}
    // delete an entry for a file in the file_t

    if(delete_entry(msg->smo_name) == FS_OK){
      write(0x0, file_t_smoid);   // update file table
    }else{
      return FS_FAIL;
    }

    break;
  case FS_GET_NAME:
    if(mounted == 0 || get_name(msg->index, msg->smo_name) == FS_FAIL){
      return FS_FAIL;
    } 
    break;
  default:
    return FS_FAIL;
  }

  return FS_OK;
}

////////////////////////////////////////
// create_file_sys creates a new file_t
// were the actual file_t is located 
////////////////////////////////////////
void create_file_sys(){

  int i = 0, sect = 1, j;
  
  while(i < 32){
    
    for(j = 0; j < 12; j++){
      file_t[i].name[j] = '\0';   // empty name
    }

    file_t[i].first_sector = sect;
    sect += BLOCK_SIZE / 512;
    i++;
  }
}

///////////////////////////////////////
// create_entry
///////////////////////////////////////

int create_entry(int smo_name){

  char name[12];  // store the name here
  int i = 0, slot = -1,j;

  // get the name from the smoid

  read_mem(smo_name, 0, 3, name);

  if (name[0] == '\0'){return -1;}

  // make sure it does not exist already

  while(i < 32 && (!streq(name,file_t[i].name))){
    if (file_t[i].name[0] == '\0' && slot == -1){slot = i;}
    i++;
  }

  if(i != 32 || slot == -1){return FS_FAIL;} // the file exists or no free slot

  // found a place then... 
  for(j = 0; j < 12; j++){
    file_t[slot].name[j] = name[j];
  }
  
  return FS_OK;
}

///////////////////////////////////////
// delete_entry
///////////////////////////////////////

int delete_entry(int smo_name){

  char name[12];  // store the name here
  int i = 0;

  // get the name from the smoid

  read_mem(smo_name, 0, 3, name);

  // make sure it does exist

  while(i < 32 && (!streq(name,file_t[i].name))){i++;}

  if(i == 32){return FS_FAIL;}

  // erase it

  file_t[i].name[0] = '\0'; 
  
  return FS_OK;
}

///////////////////////////////////////
// get Name
///////////////////////////////////////

int get_name(int index, int smo_name){
  int i = index,j = 0;
  char txt_name[16];

  if(i >= 32){return FS_FAIL;}

  while(i < 32 && file_t[i].name[0] == '\0'){i++;}

  if(i == 32){return FS_FAIL;}

  // Copy the file name and send ok //

  txt_name[0] = '\n';

  for(j = 0; j < 12; j++){
    txt_name[j+1] = file_t[i].name[j];
  }

  write_mem(smo_name, 0, 4, txt_name);
  
  return FS_OK;
}

///////////////////////////////////////
// Resolve gets the name of a file
// from a smoid and returns the first
// sector on the disk
///////////////////////////////////////
int resolve(int smo_name){
  
  char name[12];  // store the name here
  int i = 0;

  // get the name from the smoid

  read_mem(smo_name, 0, 3, name);
  
  if (name[0] == '\0'){return -1;}
  
  // search all the file table
  while(i < 32 && (!streq(name,file_t[i].name))){i++;}

  if(i == 32)
    {return -1;}
  else
    {return file_t[i].first_sector;}

}

//////////////////////////////////////
// stringcmp
//////////////////////////////////////

int streq(char *s, char *t) {
  int i=0;
  char c;
  
  while((c=s[i]) == t[i]) {
    if (!c) return 1;
    i++;
  }
  
  return 0;
}

///////////////////////////////////////
// format should send a message to
// the FDC and perform a disk
// format command
///////////////////////////////////////
int format(){
  return FS_OK;
}

///////////////////////////////////////
// read reads a sector from the disk
///////////////////////////////////////
int read(unsigned char sector, int smoid){

  struct fdc_command fdc_msg;
  struct fdc_response fdc_res;
  int id;
  
  // send message o the FDC requesting the sector

      fdc_msg.op = READ_BLOCK;
      fdc_msg.block = sector;
      fdc_msg.smoid = smoid;
      fdc_msg.ret_port = FS_RESPONSE_PORT;

      send_msg(FDC_TASK, FDC_COMMAND_PORT, &fdc_msg);
      
      // busy waiting!

      while(!get_msg_count(FS_RESPONSE_PORT)){}

      // read the response

      get_msg(FS_RESPONSE_PORT, &fdc_res, &id);

      if(fdc_res.result != FDC_OK){return FS_FAIL;}

      return FS_OK;
}

////////////////////////////////////////
// write writes a sector from the disk
////////////////////////////////////////
int write(unsigned char sector, int smoid){

  struct fdc_command fdc_msg;
  struct fdc_response fdc_res;
  int id;
  
  // send message o the FDC requesting the sector

      fdc_msg.op = WRITE_BLOCK;
      fdc_msg.block = sector;
      fdc_msg.smoid = smoid;
      fdc_msg.ret_port = FS_RESPONSE_PORT;

      send_msg(FDC_TASK, FDC_COMMAND_PORT, &fdc_msg);
      
      // busy waiting!

      while(!get_msg_count(FS_RESPONSE_PORT)){}

      // read the response

      get_msg(FS_RESPONSE_PORT, &fdc_res, &id);

      if(fdc_res.result != FDC_OK){return FS_FAIL;}

      return FS_OK;
}

