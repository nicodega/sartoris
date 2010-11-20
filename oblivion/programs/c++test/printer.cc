#include "printer.h"

prt::prt() {
  
}

int prt::print(char *txt) {
  struct csl_io_msg iomsg;
  int msg_buf[4];
  int id;
  int len;

  if (!initzd) {
    get_msg(0, msg_buf, &id);
    csl_num = msg_buf[0]+8;
    initzd=true;
  }
  
  len=0;
  while(txt[len++]) ;
  
  iomsg.command = CSL_WRITE;
  iomsg.smo = share_mem(CONS_TASK, txt, len+1, READ_PERM);
  iomsg.attribute = 7;
  iomsg.response_code = 0;
  send_msg(CONS_TASK, csl_num, &iomsg);
  
  while(get_msg_count(0)==0) {
    reschedule();
  }
  
  get_msg(0, msg_buf, &id);
  
  claim_mem(iomsg.smo);

}

