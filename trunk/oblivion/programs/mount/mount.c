#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <services/filesys/filesys.h>

/////////////////////////////////////
// This is a simple mount program  //
/////////////////////////////////////

char txt_error[] = "\nCould not mount floppy disk!"; 
char txt_ok[] = "\nFloppy mounted.";

void mount()
{
  struct fs_command fs_msg;
  struct fs_response fs_res;
  struct csl_io_msg iomsg;
  int csl[4];	
  int id;
	
  open_port(0, 0, UNRESTRICTED);

  send_msg(PMAN_TASK, 5, &csl);

  while (get_msg_count(0)==0);

  get_msg(0, csl, &id);

  // Send FS message //

  fs_msg.op = FS_MOUNT;
  fs_msg.ret_port = 1;

  send_msg(FS_TASK, FS_CMD_PORT, &fs_msg);

  while(get_msg_count(1)==0);
  
  get_msg(1, &fs_res, &id);   // get response from FS

  iomsg.command = CSL_WRITE;
  iomsg.attribute = 11;
  iomsg.len = 40;
  iomsg.response_code = 0;

  if(fs_res.result != FS_OK){
      iomsg.smo = share_mem(CONS_TASK, txt_error, 10, READ_PERM);
  } else {
      iomsg.smo = share_mem(CONS_TASK, txt_ok, 10, READ_PERM);
  }

  send_msg(CONS_TASK, 8+csl[0], &iomsg);

  // whait until message is printed . if not SMO will be destroyed! //
  
  while(get_msg_count(0)==0);

  send_msg(PMAN_TASK, 4, csl);  // exit process

  while(1);
}
