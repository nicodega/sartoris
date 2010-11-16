
#include <sartoris/kernel.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>
#include <lib/printf.h>

char arguments[80];
int console;

char *args[16];

void init() {
    int init_data[4];
    int i, len;
    char c;
    int a = 0;

    open_port(0, 0, UNRESTRICTED);

    send_msg(PMAN_TASK, 5, &init_data);

    while (get_msg_count(0)==0);

    get_msg(0, init_data, &i);

    console = init_data[0];

    if ( read_mem(init_data[1], 0, 20, arguments) < 0 ) {
        printf("\nerror while reading parameter line");
        exit(1);
    }

    for (i=0; i<80; i++) {
        if (arguments[i]=='\0') {
            len = i;
            break;
        } else if (arguments[i]==' ') { 
            arguments[i] = '\0'; 
        }
    }


    if (arguments[0] != '\0') {
        args[a++] = arguments;
    }

    for (i=1; i<len; i++) {
        if ( arguments[i] == '\0' && arguments[i+1] != '\0' ) {
            args[a++] = &arguments[i+1];
        }
    }

    exit(main(a, args));

}

int exit(int ret) {
    int csl[4];

    csl[0] = ret;
    send_msg(PMAN_TASK, 4, csl);

    while(1) { reschedule(); }
}
