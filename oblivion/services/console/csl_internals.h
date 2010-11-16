/*  
 *   Oblivion 0.1 console service internals header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#include <services/console/console.h>

#define CTL_KEY_MASK 0x3c
#define CONTROL_MASK 0x0c
#define ALT_MASK 0x30

#define IBUF_SIZE 256

#define BACKSPACE 0x08

#define UP 0x90
#define DOWN 0x91
#define LEFT 0x92
#define RIGHT 0x93

struct vterm {
  int scanning_pos;
  int scanning;
  int cursor_pos;
  int input_len;
  int print_len;
  int max_input_len;
  int input_smo;
  int input_owner_id;
  char screen_buf[80*25*2];
  char input_buf[IBUF_SIZE];
  char delimiter;
  char echo;
  char modified;
  char scanning_att;
  char done;
  char response;
};

void create_keyb_thread(void);

/* main procedures */
void do_control(void);     /* receive & process terminal resets, switches, etc... */
void do_io(void);          /* receive & process terminal reads and writes         */

/* empty the keyboard queue */
void get_keystrokes(void); 

/* these operate on virtual terminals */
void vt_switch(int target);
int vt_print(int target, int pos, char *str, int len, int att, int *nl);
void vt_reset(int target);

/* auxiliary procedures that operate on our copy of the video buffer */
void virtual_string_print(int target, char *buf, int pos, int att);
void virtual_scroll_up_one(int target);
