#include "../include/sartoris/message.h"


void main(){

  struct msg_container msgc;
  int p,p1,id = 1,msg[4] = {1,2,3,4};

  init_msg(&msgc);

  p = create_port(&msgc,0);
  p1 = create_port(&msgc,1);

  enqueue(&msgc,0,p1,msg);

  msg[0] = 0;
  msg[1] = 0;
  msg[2] = 0;
  msg[3] = 0;

  dequeue(&msgc,&id,p1,msg);
  


}
