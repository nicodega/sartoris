/*
*       Console Service.
*
*       Copyright (C) 2002, 2003, 2004, 2005
*       
*       Santiago Bazerque       sbazerque@gmail.com                     
*       Nicolas de Galarreta    nicodega@gmail.com
*
*       
*       Redistribution and use in source and binary forms, with or without 
*       modification, are permitted provided that conditions specified on 
*       the License file, located at the root project directory are met.
*
*       You should have received a copy of the License along with the code,
*       if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*       or you can contact us directly at the email addresses provided above.
*
*
*/

#include <lib/debug.h>

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <os/pman_task.h>
#include <lib/const.h>
#include <lib/structures/list.h>
#include <lib/malloc.h>
#include <drivers/screen/screen.h>
#include <drivers/keyboard/keyboard.h>
#include <lib/int.h>
#include <services/stdconsole/consoleioctrl.h>
#include <services/stds/stddev.h>
#include <services/stds/stdservice.h>
#include <services/stds/stdchardev.h>
#include <services/directory/directory.h>
#include <services/pmanager/services.h>

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#define MIN(a,b) ((a > b)? b : a)
#define MAX(a,b) ((a > b)? a : b)


#define CSL_STDDEV_PORT         1
#define CSL_STDCHARDEV_PORT     2

#define CSL_FORCE_ECHO          0x1
#define CSL_FORCE_NOECHO        0x2
#define CSL_FORCE_NONE          0x0

#define CTL_KEY_MASK 0x3c
#define CONTROL_MASK 0x0c
#define ALT_MASK     0x30

#define IBUF_SIZE 256

#define BACKSPACE 0x08
#define HOME      0x02
#define END       0x03
#define DEL       0x7F
#define UP     0x90
#define DOWN   0x91
#define LEFT   0x92
#define RIGHT  0x93

#define MAX_OWNERS              20
#define MAX_SUSCRIPTIONS        16
#define MAX_MOUSE_SUSCRIPTIONS  10

#define NUM_VIRTUAL 8
#define KBD_BUF_SIZE 256  /* must match size defined in the driver */

#define TAB_SIZE 1

#define CONSOLE_ROWS 25
#define CONSOLE_COLS 80

#define MOUSE_PX_CHAR        4    // pixels per char for the mouse
#define MOUSE_RESOLUTION     0x02
#define MOUSE_SCALING        1
#define MOUSE_SAMPLING_RATE  100
#define MOUSE_MAXX ((CONSOLE_COLS-1)*MOUSE_PX_CHAR)
#define MOUSE_MAXY ((CONSOLE_ROWS-1)*MOUSE_PX_CHAR)

extern struct stdservice_res dieres;
extern int dieid;
extern int dieretport;

struct key_suscription
{
        int taskid;
        char keycodes[MAX_SUSCRIPTIONS];
        int port;
        int susc;
} PACKED_ATT;

extern int mouse_suscriptions_ports[MAX_MOUSE_SUSCRIPTIONS];
extern int mouse_suscriptions[MAX_MOUSE_SUSCRIPTIONS];
extern int msuscs;

struct vterm 
{
        int scanning_pos;
        int scanning;
        int cursor_pos;
        int input_len;
        int print_len;  
        char attribute;
        int max_input_len;
        int input_smo;
        int input_owner_id;
        char screen_buf[80*25*2];
        char input_buf[IBUF_SIZE];
        char delimiter;
        char echo;
        char modified;
        char done;

        char msg_id;
        char usedelim;
        char scanc;     // indicates whether a read or read c was issued
        char passmem;

        int command;
        int ret_port;

        int owners;
        int exclusive; // exclusive ownership won't be implemented because the ofs service treats all writes as exclusive
        int ownerid[MAX_OWNERS]; // array of device owners
        int force_echo[MAX_OWNERS];

        list suscribers;
} PACKED_ATT;

void create_keyb_thread(void);

/* main procedures */
void do_control(void);     /* receive & process terminal resets, switches, etc... */
void do_io(void);          /* receive & process terminal reads and writes         */
int process_stdservice(void); /* receive and process std service messages */
void process_stddev(void); /* receive and process std dev messages */

/* empty the keyboard queue */
void get_keystrokes(void); 

/* these operate on virtual terminals */
void vt_switch(int target);
int vt_print(int target, int pos, char *str, int len, int att, int *nl, int delimited, char delimiter);
void vt_reset(int target);

/* auxiliary procedures that operate on our copy of the video buffer */
void virtual_string_print(int target, char *buf, int pos, int att);
void virtual_scroll_up_one(int target);

int get_size(struct stdchardev_cmd *iomsg, char *str, int writec, int len);
void signal(char c, char mask);
void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
void swap_array(int *array, int start, int length);
struct key_suscription *get_suscription(int sender_id, int console);
int remove_suscription(int sender_id, int console);
void finish_read(int i);
int check_ownership(int term, int task);

/* mouse */
void create_mouse_thread(void);
void do_mouse();
void mouse_restore();
void mouse_print();
void mouse_reset_pos();
void mouse_enable(int enabled);
void init_mouse();
