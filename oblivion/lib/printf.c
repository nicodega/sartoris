#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>
#include <lib/printf.h>

/* This is a simple printf implementation for Oblivion. It should be useful as 
 * a starting point for a future serious one. I have not implemented snprintf
 * or vsnprintf, and those are a must. Bear in mind that passing a long 
 * expression to printf will certainly crash, as I used a buffer in the stack.
 */

enum { EXPAND, VERBATIM }; 

int i2a(int x, char *s);
int i2ax(int i, char *s);
int strcp(char *str, char *buf);

extern int console;

int printf(char *format, ...) {
	struct csl_io_msg iomsg;
	int buf_smo;
	char buf[512];
	int msg_buf[4];	
	int i;
	
	/* call vsprintf to construct string */
	vsprintf(buf, format, (int*) (&format + 1));
	
	/* empty port zero */	
	while (get_msg_count(0)) { get_msg(0, msg_buf, &i); }
	
	/* issue print command */
	iomsg.command = CSL_WRITE;
	iomsg.smo = share_mem(CONS_TASK, buf, 128, READ_PERM);
	iomsg.attribute = 7;
	iomsg.len = 512;
	iomsg.response_code = 0;
	send_msg(CONS_TASK, 8+console, &iomsg);
	
	/* wait for acknowledge */
	while (get_msg_count(0)==0) { reschedule(); } 
	
	get_msg(0, msg_buf, &i);
	
	return 0;	
	
}

int sprintf(char *str, char *format, ...) {
	return vsprintf(str, format, (int*) (&format + 1));
}

int vsprintf(char *str, char *format, int *args) {
	int a, i, j;
	char c;
	int state;	
	
	i=j=a=0;
	state = VERBATIM;
	while(c = *format++) {
		switch (state) {
		case VERBATIM:
			switch (c) {
			case '%':
				state = EXPAND;
				break;
			default:
				str[j++] = c;
				break;
			}
			break;
		case EXPAND:
			switch (c) {
			case '%':
				str[j++] = c;
				break;
			case 'd': case 'D': case 'i': case 'I':
				j += i2a(args[a++], &str[j]);
				break;
			case 'x': case 'X':
				j += i2ax(args[a++], &str[j]);
				break;
			case 's': case 'S':
				j += strcp((char *) args[a++], &str[j]);
			}
			state = VERBATIM;
			break;
		}
	}
	
	str[j] = '\0';
	return 0; 
	
}

int i2a(int x, char *s) {
	char digits[10];
	int i=0, j=0;
	int retval;
	
	do {
		digits[i++] = x % 10;
		x = x / 10;
	} while (x);
	
	retval = i; 	
	
	while (i--) {
		s[j++] = '0' + digits[i];
	}
	
	return retval;
}

int i2ax(int x, char *s) {
	char digits[8];
	int i=0, j=0;
	int retval;
	char c;
	
	do {
		digits[i++] = x % 16;
		x = x / 16;
	} while (x);
	
	s[0] = '0';
	s[1] = 'x';
	
	retval = i+2;
		
	while (i--) {
	  c = digits[i];
	  if (c < 10) {
	    c += '0';
	  } else {
	    c = 'a' - 10 + c;
	  }
	  s[(j++) + 2] = c;
	}
	
	return retval;
}

/* don't copy the \0 */

int strcp(char *str, char *buf) {
	int i=0;
	char c;
	
	while( c=str[i] ) { buf[i++]=c; }
	
	return i;
}
