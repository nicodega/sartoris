#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <services/filesys/filesys.h>
#include <lib/scheduler.h>

////////////////////////////////////
// This is a simple 'ls' program  //
////////////////////////////////////

char name[16];

void list()
{

  struct fs_command fs_msg;
  struct fs_response fs_res;
  struct csl_io_msg iomsg;
  int csl[4], msg[4];	
  int id, fcount = 0;

  open_port(0, 0, UNRESTRICTED);

  name[0] =  '\n';
  name[1] = '.';
  name[2] = '\0';

  send_msg(PMAN_TASK, 5, &csl);

  while (get_msg_count(0)==0);

  get_msg(0, csl, &id);

  // Send FS messages //

  fs_msg.smo_name = share_mem(FS_TASK, name, 4, WRITE_PERM);

  iomsg.command = CSL_WRITE;
  iomsg.attribute = 11;
  iomsg.len = 12;
  iomsg.response_code = 0;
  iomsg.smo = share_mem(CONS_TASK, name, 4, READ_PERM);

  do {
    
    send_msg(CONS_TASK, 8+csl[0], &iomsg);  // print file name
    while(get_msg_count(CSL_ACK_PORT)==0) { reschedule(); }// wait for console ack  
    get_msg(CSL_ACK_PORT, msg, &id);   // get response from console

    fs_msg.op = FS_GET_NAME;
    fs_msg.ret_port = 1;
    fs_msg.index = fcount;
  
    send_msg(FS_TASK, FS_CMD_PORT, &fs_msg);
    while(get_msg_count(1)==0) { reschedule(); }// wait for name
    get_msg(1, &fs_res, &id);   // get response from FS
    
    fcount++;

  } while (fs_res.result != FS_FAIL) ;

  send_msg(PMAN_TASK, 4, csl);  // exit process

  while(1) { reschedule(); }
}
