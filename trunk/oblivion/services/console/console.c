/*  
*   Oblivion console service
*   
*   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
*   
*   sbazerqu@dc.uba.ar                
*   nicodega@cvtci.com.ar
*/

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <oblivion/layout.h>
#include <drivers/screen/screen.h>
#include <drivers/keyboard/keyboard.h>
#include <lib/int.h>
#include <lib/reboot.h>
#include "csl_internals.h"

#include <lib/print.h>

#define NUM_VIRTUAL 8
#define KBD_BUF_SIZE 256  /* must match size defined in the driver */

#define TAB_SIZE 1

struct vterm t[NUM_VIRTUAL];

char kbd_buf[KBD_BUF_SIZE];

int cur_screen;
volatile int full_buf;
volatile int next_read;
volatile int next_write;

char str[MAX_CSL_WRITE];

char txt_reboot[] = "\n\n-> console server: the system is going down for reboot NOW!";
char txt_bad_smo_in[] = "\n\n-> console server: bad SMO received for console print.\n\n";
char txt_bad_smo_out[] = "\n\n-> console server: bad SMO received for console scan.\n\n";

void console(void) 
{
    int i;
    struct csl_signal_msg msg;

    open_port(0, 0, UNRESTRICTED);
    for(i = 0; i < NUM_VIRTUAL; i++)
        open_port(1+i, 0, UNRESTRICTED);

    /* Let PMAN know we are alive */
    msg.term = -1;
    send_msg(0, 1, &msg);
    
    /* initialization code */
    __asm__ __volatile__ ("sti" : : );
    
    cur_screen=0;
    next_read=next_write=0;
    full_buf=0;

    for (i=0; i<NUM_VIRTUAL; i++) 
    {
        vt_reset(i);
    }

    create_keyb_thread();
    
    /* ok, we spawned the thread that will handle the keyboard. */
    /* now enter the main loop. */
int k =0;
    for(;;) {
        string_print("CON ALIVE",22*160,k++);
        do_io();

        run_thread(SCHED_THR);
    }
}

void create_keyb_thread(void) {
    struct thread kbd_hdl;

    kbd_hdl.task_num=CONS_TASK;
    kbd_hdl.invoke_mode = PRIV_LEVEL_ONLY;
    kbd_hdl.invoke_level = 0;
    kbd_hdl.ep= (void*)&keyb_int_thread_entry;
    kbd_hdl.stack= (void*)(0xe000 - 0x4);

    create_thread(CONSK_THR, &kbd_hdl);
    create_int_handler(33, CONSK_THR, 1, 1);
}

void do_io(void) 
{
    struct csl_io_msg iomsg;
    struct csl_response_msg rmsg;
    int i, j, ret_len, id, len;
    
    /* process new requests */
    for (i=0; i<NUM_VIRTUAL; i++) 
    {
        while (!t[i].scanning && get_msg_count(1+i) > 0) 
        {   
            get_msg(1+i, &iomsg, &id);
            
            switch(iomsg.command) 
            {
              case CSL_RESET:
                  //vt_reset(i);
                  break;
              case CSL_SWITCH:
                  vt_switch(i);
                  break;
              case CSL_WRITE:
                  len = mem_size(iomsg.smo);
                  if (len > MAX_CSL_WRITE) {
                      len = MAX_CSL_WRITE;
                  }
                  if (len == -1 || read_mem(iomsg.smo, 0, len, &str) >=0) 
                  {
                      t[i].scanning_pos = vt_print(i, t[i].scanning_pos,
                          str, len, iomsg.attribute, &j);

                      if (i==cur_screen) { 
                          set_cursor_pos(t[i].scanning_pos/2); 
                      }

                      rmsg.code = iomsg.response_code;
                      rmsg.smo = iomsg.smo;
                      send_msg(id, 0, &rmsg);

                  } else {
                      t[cur_screen].scanning_pos = vt_print(cur_screen,
                          t[cur_screen].scanning_pos,
                          txt_bad_smo_in,
                          160,
                          0x7,
                          &j);
                      set_cursor_pos(t[cur_screen].scanning_pos/2); 
                  }
                  break;

              case CSL_READ:
                  len = mem_size(iomsg.smo);
                  t[i].scanning = 1;
                  if (len <= IBUF_SIZE) {
                      t[i].max_input_len = len-1;
                  } else {
                      t[i].max_input_len = IBUF_SIZE-1;
                  }

                  t[i].input_owner_id = id;
                  t[i].input_smo = iomsg.smo;
                  t[i].response = iomsg.response_code;
                  t[i].delimiter = iomsg.delimiter;	
                  t[i].done = 0;
                  t[i].input_len = 0;
                  t[i].print_len = 0;
                  t[i].cursor_pos = 0;
                  t[i].modified = 0;

                  if (t[i].echo = iomsg.echo) {
                      t[i].scanning_att = iomsg.attribute;
                      if (i==cur_screen) { cursor_on(); }
                  }

                  break;
            }
        }    
    }

    /* get new keystrokes */
    get_keystrokes();

    /* see if any scans are done and do echo */
    for (i=0; i<NUM_VIRTUAL; i++) {
        if (t[i].scanning && t[i].modified) {

            t[i].modified = 0;

            if (t[i].echo) {
                vt_print(i, t[i].scanning_pos, t[i].input_buf,
                    t[i].print_len, t[i].scanning_att, &j);
                t[i].scanning_pos -= j*80*2;
                if (i==cur_screen) { 
                    set_cursor_pos(t[i].scanning_pos/2 + t[i].cursor_pos);
                }
            }	  

            if (t[i].done) {

                t[i].scanning=0;
                ret_len = t[i].input_len;
                t[i].input_buf[ret_len++]=0;

                if ( write_mem(t[i].input_smo, 0, ret_len+1, t[i].input_buf) < 0 ) 
                {
                        t[cur_screen].scanning_pos = vt_print(cur_screen,
                            t[cur_screen].scanning_pos,
                            txt_bad_smo_out,
                            160,
                            0x7,
                            &j);
                        set_cursor_pos(t[cur_screen].scanning_pos/2); 
                } 

                /* send ack */
                rmsg.code = t[i].response;
                rmsg.smo = t[i].input_smo;
                send_msg(t[i].input_owner_id, CSL_ACK_PORT, &rmsg);

                if (t[i].echo) {
                    cursor_off();
                    t[i].scanning_pos += 2*t[i].print_len;
                    t[i].print_len = t[i].cursor_pos = 0;
                }

            }
        }
    }

}  

void get_keystrokes(void) 
{
    unsigned char mask, c;
    int i, j;
    struct vterm *cterm;
    struct csl_signal_msg signal;

    while (! (next_read==next_write && !full_buf)) 
    {
        mask = kbd_buf[next_read];
        c = kbd_buf[next_read+1];
        if ( mask & CTL_KEY_MASK) 
        {
            if (mask & CONTROL_MASK) 
            {
                signal.term = cur_screen;
                signal.key = c;
                signal.alt = mask & ALT_MASK;
                send_msg(PMAN_TASK, CSL_SGN_PORT, &signal);
            } 
            else if ( mask & ALT_MASK) 
            {
                if (c == LEFT) 
                {   /* ALT+4 */
                    if (cur_screen==0) 
                    {
                        i = NUM_VIRTUAL - 1;
                    } else {
                        i = cur_screen-1;
                    }
                    vt_switch(i);
                } 
                else if (c == RIGHT) 
                {
                    i = (cur_screen+1) % NUM_VIRTUAL;
                    vt_switch(i);
                }
            }
        } 
        else 
        {  /* !(mask & CTL_KEY_MASK) */
            cterm = &t[cur_screen];
            if (cterm->scanning) 
            {
                cterm->modified = 1;

                if (c==BACKSPACE) 
                {                  /* we eat the backspaces here */
                    if (cterm->cursor_pos > 0) 
                    {
                        for (i=cterm->cursor_pos; i<cterm->input_len; i++) 
                        {
                            cterm->input_buf[i-1] = cterm->input_buf[i];
                        }
                        cterm->input_buf[--cterm->input_len] = ' ';
                        cterm->cursor_pos--;
                    }
                } 
                else if (c==LEFT) 
                {
                    if (cterm->cursor_pos > 0) 
                    {
                        cterm->cursor_pos--;
                    }
                } 
                else if (c==RIGHT) 
                {
                    if (cterm->cursor_pos < cterm->input_len) 
                    {
                        cterm->cursor_pos++;
                    }
                } 
                else if (c==cterm->delimiter) 
                {
                    cterm->done=1;
                }
                else if (c < 0x80) 
                {
                    if (cterm->input_len < cterm->max_input_len) 
                    {
                        for (i=cterm->input_len; i>cterm->cursor_pos; i--) 
                        {
                            cterm->input_buf[i] = cterm->input_buf[i-1];
                        }
                        cterm->input_buf[cterm->cursor_pos++] = c;
                        if(++cterm->input_len > cterm->print_len) 
                        {
                            cterm->print_len = cterm->input_len;
                        }
                    }
                }
            }
        }

        asm ( "cli" : : );

        next_read = (next_read+2) % KBD_BUF_SIZE;
        full_buf=0;

        asm ( "sti" : : );
    }
}


void vt_switch(int i) {
    read_screen(t[cur_screen].screen_buf);
    write_screen(t[cur_screen=i].screen_buf);
    if (t[i].scanning) {
        cursor_on();
        set_cursor_pos(t[i].scanning_pos/2 + t[i].cursor_pos);
    } else {
        cursor_off();
        set_cursor_pos(t[i].scanning_pos/2);
    }
}


void vt_reset(int i) 
{
    int j=0;

    while(j<80*25*2) 
    {
        t[i].screen_buf[j++]=' ';
        t[i].screen_buf[j++]=0x07;
    }

    t[i].scanning=0;
    t[i].scanning_pos=0;
    t[i].cursor_pos=0;
    t[i].print_len=0;
    t[i].input_len=0;
    if (i==cur_screen) 
    {
        cursor_off();  
        write_screen(t[i].screen_buf); 
    }
}


int vt_print(int target, int pos, char *str, int len, int att, int *nl) {
    char buf[81];
    int i, j, k;
    int rem;

    i = 0;
    *nl=0;
    while (i<len && str[i]!=0) {
        j=0;
        rem = 80 - ( ((pos>>1)) % 80 );
        while(j<rem && i<len && str[i]!=0) {
            if (str[i]=='\n') {
                i++;
                while(j<rem) { buf[j++]=' '; }
            } else if (str[i]=='\t') {
                i++;
                k=0;
                while(j<rem && k<TAB_SIZE) {
                    buf[j++]=' ';
                    k++;
                }
            } else {
                buf[j++]=str[i++];
            }
        }

        buf[j]=0;
        if (target==cur_screen) {
            string_print(buf, pos, att);
        } else {
            virtual_string_print(target, buf, pos, att);
        }
        pos += j*2;

        if(pos>=80*25*2) {
            (*nl)++;
            pos=80*24*2;
            if (target==cur_screen) {
                scroll_up_one();
            } else {
                virtual_scroll_up_one(target);
            }
        }
    }

    return pos;
}

void virtual_string_print(int target, char *buf, int pos, int att) {
    char *vbuf;
    int i;

    vbuf = t[target].screen_buf;

    i=0;
    while (buf[i]) {
        vbuf[pos++] = buf[i++];
        vbuf[pos++] = (char) att;
    }
}

void virtual_scroll_up_one(int target) {
    int i;
    char *vbuf;

    vbuf = t[target].screen_buf;

    for(i=0; i<80*24*2; i++) {
        vbuf[i] = vbuf[i+80*2];
    }

    i=80*24*2;
    while(i<80*25*2) {
        vbuf[i++] = ' ';
        vbuf[i++] = 0x07;
    }
}
