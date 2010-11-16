#include "printer.h"

int main(void) {
	prt P;
    int msg[4], i;

    send_msg(PMAN_TASK, 5, &msg);

    while (get_msg_count(0)==0);

    get_msg(0, msg, &i);
		
	P.print("\nhello\n");
	
	while(1);
}

