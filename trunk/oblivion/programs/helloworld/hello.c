#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>

char helloworld[] = "\nhello, world!";

void run(void) {
	struct csl_io_msg iomsg;
	int csl[4];	
	int i;

    open_port(0, 0, UNRESTRICTED);

    send_msg(PMAN_TASK, 5, &csl);
	
	while (get_msg_count(0)==0);

	get_msg(0, csl, &i);

	iomsg.command = CSL_WRITE;
	iomsg.smo = share_mem(CONS_TASK, helloworld, 40, READ_PERM);
	iomsg.attribute = 11;
	iomsg.response_code = 0;
	send_msg(CONS_TASK, 8+csl[0], &iomsg);

	// wait for messages to avoid SMO destruction //

	while(get_msg_count(0)==0) {
	  reschedule();
	}
	
	send_msg(PMAN_TASK, 4, csl);
	
	while(1) {reschedule();}
} 
	
