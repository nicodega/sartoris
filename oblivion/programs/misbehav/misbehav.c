#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>

#define ovflw

#ifdef gpf
char byeworld[] = "\ni will produce a GPF right after this print request!";
#endif

#ifdef div0
char byeworld[] = "\ni will produce a divide by 0 right after this print request!";
#endif

#ifdef ovflw
char byeworld[] = "\ni will produce an overflow right after this print request!";
#endif

char lazarus[] = "\nthe process manager has saved me! press ctrl-C to terminate me.";

void run(void) 
{
	struct csl_io_msg iomsg;
	int csl[4];	
	int i, j;

    open_port(0, 0, UNRESTRICTED);
	
    send_msg(PMAN_TASK, 5, &csl);

    while (get_msg_count(0)==0) ;

	get_msg(0, csl, &i);

	iomsg.command = CSL_WRITE;
	iomsg.smo = share_mem(CONS_TASK, byeworld, 20, READ_PERM);
	iomsg.attribute = 11;
	iomsg.len = 80;
	iomsg.response_code = 0;
	send_msg(CONS_TASK, 8+csl[0], &iomsg);
	
	

	while(get_msg_count(0)==0) ;
	
 sum:	
	//OVERFLOW
#ifdef ovflw	
	i = 0x8fffffff;
	j = 0x8fffffff;
	__asm__ ("add %0, %1" : "=r" (i) : "r"(j), "0"(i) );
	__asm__ ("into" : : );
#endif   
	//DIV BY ZERO
#ifdef div0
	i = 0;
	i = 7 / i;
#endif
	
	// GPF
#ifdef gpf
	i = *(int*)(0xfffffff0);
#endif
	
	iomsg.command = CSL_WRITE;
	iomsg.smo = share_mem(CONS_TASK, lazarus, 20, READ_PERM);
	iomsg.attribute = 11;
	iomsg.len = 80;
	iomsg.response_code = 0;
	send_msg(CONS_TASK, 8+csl[0], &iomsg);
	
	while(get_msg_count(0)==1) ;
	
	//send_msg(PMAN_TASK, 4, csl);
	
	while(1);
}
