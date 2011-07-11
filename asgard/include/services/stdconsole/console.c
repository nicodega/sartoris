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

#include "csl_internals.h"
#include <os/pman_task.h>
#include <lib/scheduler.h>

struct stdservice_res dieres;
int dieid, dieretport;

unsigned char kbd_buf[KBD_BUF_SIZE];
struct vterm t[NUM_VIRTUAL];

int cur_screen;
volatile int full_buf;
volatile int next_read;
volatile int next_write;

char txt_reboot[] = "\n\n-> console server: the system is going down for reboot NOW!";
char txt_bad_smo_in[] = "\n\n-> console server: bad SMO received for console print.\n\n";
char txt_bad_smo_out[] = "\n\n-> console server: bad SMO received for console scan.\n\n";

char *service_name = "devices/tty";
unsigned char mbuffer[1024 * 5]; // 5k

void console(void) 
{
    struct directory_register reg_cmd;
    struct directory_response dir_res;
    int i, j, id_proc, die = 0;

    // open ports with permisions for services only (lv 0, 1, 2) //
    open_port(3, 2, PRIV_LEVEL_ONLY);
    open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_STDDEV_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(CSL_STDCHARDEV_PORT, -1, UNRESTRICTED);

    /* initialization code */
    __asm__ ("sti" : : );

    // register with directory as tty
    reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
    reg_cmd.ret_port = 3;
    reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);

    while(send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd) < 0)
    { 
        reschedule(); 
    }

    while (get_msg_count(3) == 0){ reschedule(); }

    get_msg(3, &dir_res, &id_proc);

    close_port(3);

    claim_mem(reg_cmd.service_name_smo);

    cur_screen=0;
    next_read = next_write = 0;
    full_buf=0;
    init_mem(mbuffer, 1024 * 5);

    for (i=0; i<NUM_VIRTUAL; i++) 
    {
        vt_reset(i);

        init(&t[i].suscribers);
        t[i].owners = 0;
        t[i].exclusive = 0;
        t[i].max_input_len = 0;
        t[i].input_smo = -1;
        t[i].input_owner_id = -1;
        t[i].delimiter = '\0';
        t[i].echo = 1;
        t[i].modified = 0;
        t[i].done = 0;
        t[i].msg_id = 0;
        t[i].usedelim = 1;
        t[i].scanc = 0;
        t[i].passmem = 0;
        t[i].command = -1;
        t[i].ret_port = -1;

        for(j = 0; j < MAX_OWNERS; j++)
        {
            t[i].ownerid[j] = -1; 
            t[i].force_echo[j] = CSL_FORCE_NONE;
        }
    }

    create_keyb_thread();

    init_mouse();

    /* ok, we spawned the thread that will handle the keyboard. */
    /* now enter the main loop. */
    while(!die) 
    {
        string_print("CON ALIVE",6*160-18,i++);

        die = process_stdservice();

        process_stddev();

        do_io(); 

        do_mouse();

        reschedule();
    }

    string_print("CONS: DIED ",0,7);

    send_msg(dieid, dieretport, &dieres);
    for(;;);
}

void create_keyb_thread(void) 
{
    /* INT 33 */
    struct pm_msg_create_thread msg_create_thr;
    struct pm_msg_response      msg_res;
    int port = 3, sender_id = 0;

    msg_create_thr.pm_type = PM_CREATE_THREAD;
    msg_create_thr.req_id = 0;
    msg_create_thr.response_port = port;
    msg_create_thr.task_id = get_current_task();    
	msg_create_thr.stack_addr = NULL;
    msg_create_thr.entry_point = &keyb_int_thread_entry;
    msg_create_thr.interrupt = 33;  // int 1
    msg_create_thr.int_priority = 1;

    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

    while (get_msg_count(port) == 0){reschedule();}

    get_msg(port, &msg_res, &sender_id);
}

int get_size(struct stdchardev_cmd *iomsg, char *str, int writec, int max_len)
{
    int i = 0;

    if(writec) return 1;

    if(iomsg->flags & CHAR_STDDEV_READFLAG_DELIMITED)
    {
        while(i < iomsg->byte_count && str[i] != iomsg->delimiter && i < max_len){i++;}

        if(str[i] == iomsg->delimiter) return i+1;
    }

    return iomsg->byte_count;
}

void do_io(void) 
{
    struct stdchardev_cmd iomsg;
    struct stdchardev_res rmsg;
    int i, j, ret_len, id, newpos, writec;
    char *pstr = NULL;

    /* process new requests */
    while (get_msg_count(CSL_STDCHARDEV_PORT) > 0) 
    {
        get_msg(CSL_STDCHARDEV_PORT , &iomsg, &id);

        rmsg.msg_id = iomsg.msg_id;
        rmsg.command = iomsg.command;
        rmsg.byte_count = 0;
        rmsg.dev = iomsg.dev;

        if(!check_ownership(iomsg.dev, id) || iomsg.dev >= NUM_VIRTUAL )
        {
            if((iomsg.command == CHAR_STDDEV_READ || iomsg.command == CHAR_STDDEV_WRITE) && (iomsg.flags & CHAR_STDDEV_READFLAG_PASSMEM)) 
                pass_mem(iomsg.buffer_smo, id);
            rmsg.ret = STDCHARDEV_ERR;
            send_msg(id, iomsg.ret_port, &rmsg);
            continue;
        }

        switch(iomsg.command) 
        {
            case CHAR_STDDEV_RESET:
                if(t[iomsg.dev].scanning)
                {
                    // if scanning fail
                    rmsg.ret = STDCHARDEV_ERR;
                    send_msg(id, iomsg.ret_port, &rmsg);
                    continue;
                }
                vt_reset(iomsg.dev);
                rmsg.ret = STDCHARDEV_OK;
                send_msg(id, iomsg.ret_port, &rmsg);
                break;
            case CHAR_STDDEV_WRITEC:
            case CHAR_STDDEV_WRITE:
                writec = (iomsg.command == CHAR_STDDEV_WRITEC);

                if(!writec)
                    pstr = (char*)malloc( MIN(mem_size(iomsg.buffer_smo), iomsg.byte_count) );
                else
                    pstr = (char*)&((struct stdchardev_writec_cmd*)&iomsg)->c;

                if (writec || read_mem(iomsg.buffer_smo, 0, iomsg.byte_count, pstr) == 0) 
                {
                    newpos = get_size(&iomsg, pstr, writec, (writec)? 1 : MIN(mem_size(iomsg.buffer_smo), iomsg.byte_count) );

                    if(t[iomsg.dev].scanning && t[iomsg.dev].input_owner_id == id)
                    {
                        newpos = MIN(newpos, t[iomsg.dev].max_input_len - t[iomsg.dev].input_len);

                        rmsg.byte_count = newpos;

                        i = newpos - 1;
                        while(i > 0 && pstr[i] == '\0'){i--;};
                        newpos = i+1;

                        i = t[iomsg.dev].cursor_pos;
                        while(i < newpos)
                        {
                            t[iomsg.dev].input_buf[t[iomsg.dev].cursor_pos + i] = pstr[i];
                            i++;
                        }

                        t[iomsg.dev].input_len += newpos; 
                        if(t[iomsg.dev].print_len < t[iomsg.dev].input_len ) 
                            t[iomsg.dev].print_len = t[iomsg.dev].input_len; 
                        t[iomsg.dev].cursor_pos += newpos;

                        t[iomsg.dev].modified = 1;

                        if(t[iomsg.dev].input_len == t[iomsg.dev].max_input_len)
                        {
                            // finish reading       
                            t[iomsg.dev].done = 1;
                        }

                    }
                    else
                    {
                        rmsg.byte_count = newpos;

                        t[iomsg.dev].scanning_pos = vt_print(iomsg.dev, t[iomsg.dev].scanning_pos, pstr, rmsg.byte_count, t[iomsg.dev].attribute, &j, !writec && (iomsg.flags & CHAR_STDDEV_READFLAG_DELIMITED), iomsg.delimiter);

                        if (iomsg.dev==cur_screen && !t[iomsg.dev].scanning) 
                        { 
                            set_cursor_pos(t[iomsg.dev].scanning_pos/2); 
                        }
                    }

                    // pass smo
                    if(!writec && (iomsg.flags & CHAR_STDDEV_READFLAG_PASSMEM)) 
                        pass_mem(iomsg.buffer_smo, id);

                    // if current screen is scanning, write affects input
                    rmsg.ret = STDCHARDEV_OK;
                    send_msg(id, iomsg.ret_port, &rmsg);

                } 
                else 
                {
                    if(!writec && (iomsg.flags & CHAR_STDDEV_READFLAG_PASSMEM)) 
                        pass_mem(iomsg.buffer_smo, id);
                    rmsg.ret = STDCHARDEV_ERR;
                    send_msg(id, iomsg.ret_port, &rmsg);
                }
                if(!writec && pstr != NULL) 
                    free(pstr);
                break;
            case CHAR_STDDEV_TELL:
                if(!t[iomsg.dev].scanning || t[iomsg.dev].input_owner_id != id)
                {
                    rmsg.ret = STDCHARDEV_ERR;
                    send_msg(id, iomsg.ret_port, &rmsg);
                    continue;
                }
                rmsg.ret = STDCHARDEV_OK;
                rmsg.value1 = t[iomsg.dev].cursor_pos;
                send_msg(id, iomsg.ret_port, &rmsg);
                break;
            case CHAR_STDDEV_SEEK:
                if(!t[iomsg.dev].scanning || t[iomsg.dev].input_owner_id != id)
                {
                    rmsg.ret = STDCHARDEV_ERR;
                    send_msg(id, iomsg.ret_port, &rmsg);
                    continue;
                }

                // seek on char buffer
                if(iomsg.flags == CHAR_STDDEV_SEEK_CUR)
                {
                    newpos = MIN(MAX(0, t[iomsg.dev].cursor_pos + ((struct stdchardev_seek_cmd *)&iomsg)->pos), t[iomsg.dev].max_input_len);
                }
                else if(iomsg.flags == CHAR_STDDEV_SEEK_END)
                {
                    newpos = MIN(MAX(0,t[iomsg.dev].input_len - ((struct stdchardev_seek_cmd *)&iomsg)->pos), t[iomsg.dev].max_input_len);
                }
                else
                {
                    newpos = MIN(((struct stdchardev_seek_cmd *)&iomsg)->pos, t[iomsg.dev].max_input_len);
                }

                if(newpos == t[iomsg.dev].input_len)
                {
                    rmsg.ret = STDCHARDEV_OK;
                    send_msg(id, iomsg.ret_port, &rmsg);
                    continue;
                }
                else if(newpos < t[iomsg.dev].input_len)
                {
                    i = newpos;
                    while(i < t[iomsg.dev].input_len)
                    {
                        t[iomsg.dev].input_buf[i] = ' ';
                        i++;
                    }
                    t[iomsg.dev].input_len = t[iomsg.dev].cursor_pos = newpos;
                }
                else if(newpos > t[iomsg.dev].input_len)
                {
                    // append spaces
                    i = t[iomsg.dev].cursor_pos;
                    while(i < newpos)
                    {
                        t[iomsg.dev].input_buf[i] = ' ';
                        i++;
                    }

                    t[iomsg.dev].input_len = t[iomsg.dev].print_len = t[iomsg.dev].cursor_pos = newpos;
                }

                if(newpos == t[iomsg.dev].max_input_len)
                {
                    // finish reading       
                    t[iomsg.dev].done = 1;
                }

                t[iomsg.dev].modified = 1;

                rmsg.ret = STDCHARDEV_OK;
                send_msg(id, iomsg.ret_port, &rmsg);

                break;
            case CHAR_STDDEV_READC:
            case CHAR_STDDEV_READ:
                if(t[iomsg.dev].scanning)
                {
                    // if scanning fail
                    rmsg.ret = STDCHARDEV_ERR;
                    // pass smo
                    if(iomsg.command == CHAR_STDDEV_READ && (iomsg.flags & CHAR_STDDEV_READFLAG_PASSMEM)) 
                        pass_mem(iomsg.buffer_smo, id);

                    send_msg(id, iomsg.ret_port, &rmsg);
                    continue;
                }
                t[iomsg.dev].scanc = ((iomsg.command == CHAR_STDDEV_READC)? 1 : 0);
                t[iomsg.dev].passmem = (iomsg.flags & CHAR_STDDEV_READFLAG_PASSMEM)? 1 : 0;
                t[iomsg.dev].usedelim = (!t[iomsg.dev].scanc && (iomsg.flags & CHAR_STDDEV_READFLAG_DELIMITED))? 1 : 0;

                t[iomsg.dev].scanning = 1;                              

                if(t[iomsg.dev].scanc)
                {
                    t[iomsg.dev].max_input_len = 1;
                }
                else
                {                               
                    if (iomsg.byte_count <= IBUF_SIZE) 
                        t[iomsg.dev].max_input_len = iomsg.byte_count - ((t[iomsg.dev].usedelim)? 1 : 0);
                    else 
                        t[iomsg.dev].max_input_len = IBUF_SIZE - ((t[iomsg.dev].usedelim)? 1 : 0);
                }

                if(iomsg.flags & CHAR_STDDEV_READFLAG_BLOCKING)
                {
                    t[iomsg.dev].command = iomsg.command;
                    t[iomsg.dev].ret_port = iomsg.ret_port;
                    t[iomsg.dev].input_owner_id = id;
                    t[iomsg.dev].input_smo = iomsg.buffer_smo;
                    t[iomsg.dev].msg_id = iomsg.msg_id;
                    t[iomsg.dev].delimiter = iomsg.delimiter;       
                    t[iomsg.dev].done = 0;
                    t[iomsg.dev].input_len = 0;
                    t[iomsg.dev].print_len = 0;
                    t[iomsg.dev].cursor_pos = 0;
                    t[iomsg.dev].modified = 0;      
                    t[iomsg.dev].echo = ((iomsg.flags & CHAR_STDDEV_READFLAG_ECHO) == CHAR_STDDEV_READFLAG_ECHO);

                    if (t[iomsg.dev].echo && iomsg.dev == cur_screen) 
                        cursor_on(); 
                }
                else
                {
                    if(iomsg.command == CHAR_STDDEV_READ && t[iomsg.dev].passmem) 
                        pass_mem(iomsg.buffer_smo, id);

                    t[iomsg.dev].scanning = 0;

                    // this console service will only support blocking reads
                    rmsg.ret = STDCHARDEV_ERR;
                    send_msg(id, iomsg.ret_port, &rmsg);
                }
                break;
        }
    }    

    /* get new keystrokes */
    get_keystrokes();

    /* see if any scans are done and do echo */
    for (i=0; i<NUM_VIRTUAL; i++) 
    {
        if (t[i].scanning && t[i].modified) 
        {            
            t[i].modified = 0;

            if (t[i].echo) 
            {
                vt_print(i, t[i].scanning_pos, t[i].input_buf, t[i].print_len, t[i].attribute, &j, 0, '\0');

                t[i].scanning_pos -= j * 80 * 2;

                if(i==cur_screen) 
                    set_cursor_pos((t[i].scanning_pos >> 1) + t[i].cursor_pos);                             
            }         

            if (t[i].done) 
            {
                finish_read(i);         
            }
        }
    }
}  

void finish_read(int i)
{
    struct stdchardev_res rmsg;
    int ret_len, j;

    t[i].scanning = 0;
    t[i].done = 0;
    ret_len = t[i].input_len;
    //t[i].input_buf[ret_len++]=0; // this will be done by the fs

    if(t[i].scanc == 1)
    {
        // reading only one char
        ((struct stdchardev_readc_res*)&rmsg)->c = t[i].input_buf[0];
    }
    else
    {
        if ( write_mem(t[i].input_smo, 0, ret_len, t[i].input_buf) < 0 ) 
        {
            t[i].scanning_pos = vt_print(i, t[i].scanning_pos, txt_bad_smo_out, 160, 0x7, &j, 0, '\0');
            set_cursor_pos(t[i].scanning_pos/2);
        } 

        if(t[i].passmem) 
        {
            pass_mem(t[i].input_smo, t[i].input_owner_id);
        }
    }

    /* send ack */
    rmsg.msg_id = t[i].msg_id;
    rmsg.command = t[i].command;
    rmsg.byte_count = ret_len;
    rmsg.ret = STDCHARDEV_OK;
    rmsg.dev = i;

    send_msg(t[i].input_owner_id, t[i].ret_port, &rmsg);

    if (t[i].echo) 
    {
        cursor_off();
        t[i].scanning_pos += 2 * t[i].print_len;
        t[i].print_len = t[i].cursor_pos = 0;
    }       
}

void get_keystrokes(void) 
{
    unsigned char mask, c;
    int i, j;
    struct vterm *cterm = NULL;

    while (!(next_read == next_write && !full_buf)) 
    {                       
        mask = kbd_buf[next_read];
        c = kbd_buf[next_read+1];
                
        if ( mask & ALT_MASK) 
        {                       
            if (c == LEFT) 
            {
                if (cur_screen == 0) 
                {
                    i = NUM_VIRTUAL - 1;
                }
                else 
                {
                    i = cur_screen-1;
                }
            } 
            else if (c == RIGHT) 
            {
                i = (cur_screen+1) % NUM_VIRTUAL;
            }
            vt_switch(i);
        }
        else // !(mask & ALT_MASK) 
        {  
            cterm = &t[cur_screen];
                        
            if (cterm->scanning && !cterm->done) 
            {
                cterm->modified = 1;

                if (c==BACKSPACE) 
                {                  
                    // we eat the backspaces here //
                    if (cterm->cursor_pos > 0) 
                    {
                        for (i = cterm->cursor_pos; i < cterm->input_len; i++) 
                        {
                            cterm->input_buf[i-1] = cterm->input_buf[i];
                        }
                        cterm->input_buf[--cterm->input_len] = ' ';
                        cterm->cursor_pos--;
                    }
                } 
                else if (c==DEL) 
                {                  
                    // we eat the chars in front here //
                    if (cterm->cursor_pos < cterm->input_len) 
                    {
                        cterm->cursor_pos++;
                        for (i = cterm->cursor_pos; i < cterm->input_len; i++) 
                        {
                            cterm->input_buf[i-1] = cterm->input_buf[i];
                        }
                        cterm->input_buf[--cterm->input_len] = ' ';
                        cterm->cursor_pos--;
                    }
                } 
                else if (c==HOME) 
                {
                    cterm->cursor_pos = 0;
                } 
                else if (c==END) 
                {
                    cterm->cursor_pos = cterm->input_len;
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
                else if (c==cterm->delimiter && cterm->usedelim) 
                {
                    cterm->input_buf[cterm->input_len] = c;
                    cterm->input_len++;
                    cterm->done=1;
                }
                else if (c < 0x80) 
                {
                    if (cterm->input_len < cterm->max_input_len) 
                    {
                        for (i = cterm->input_len; i > cterm->cursor_pos; i--) 
                        {
                            cterm->input_buf[i] = cterm->input_buf[i-1];
                        }

                        cterm->input_buf[cterm->cursor_pos++] = c;

                        if(++cterm->input_len > cterm->print_len) 
                                cterm->print_len = cterm->input_len;                        
                    }
                    // if max len has been reached finish anyway
                    if(cterm->input_len == cterm->max_input_len)
                            cterm->done=1;
                }
            }
        }

        // process signals
        signal(c, mask);

        asm ( "cli" : : );

        next_read = (next_read+2) % KBD_BUF_SIZE;
        full_buf=0;

        asm ( "sti" : : );
    }
}


void vt_switch(int i) 
{
    mouse_restore();
    read_screen(t[cur_screen].screen_buf);
    write_screen(t[cur_screen=i].screen_buf);
    mouse_print();

    if (t[i].scanning) 
    {
        cursor_on();
        set_cursor_pos(t[i].scanning_pos/2 + t[i].cursor_pos);
    }
    else 
    {
    cursor_off();
        set_cursor_pos(t[i].scanning_pos/2);
    }
}


void vt_reset(int i) 
{
    int j = 0;
  
    while(j < 80*25*2) 
    {
        t[i].screen_buf[j++]=' ';
        t[i].screen_buf[j++]=0x07;
    }
        
    t[i].scanning=0;
    t[i].scanning_pos=0;
    t[i].cursor_pos=0;
    t[i].print_len=0;
    t[i].input_len=0;
    t[i].attribute = 7;
    if (i==cur_screen) 
    {
        cursor_off();
        write_screen(t[i].screen_buf);
    }
}

int vt_print(int target, int pos, char *str, int len, int att, int *nl, int delimited, char delimiter) 
{
    char buf[81];
    int i, j, k;
    int rem;

    i = 0;
    *nl=0;
        
    if (target==cur_screen) mouse_restore();

    while (i < len && (!delimited || (delimited && str[i] != delimiter))) 
    {
        // print a line
        j=0;
        rem = 80 - (((pos>>1)) % 80 );

        while(j < rem && i < len && (!delimited || (delimited && str[i] != delimiter))) 
        {
            if(str[i]=='\n') 
            {
                i++;
                while(j < rem) { buf[j++]=' '; }
            } 
            else if (str[i]=='\t') 
            {
                i++;
                k=0;
                while(j < rem && k < TAB_SIZE) 
                {
                        buf[j++]=' ';
                        k++;
                }
            }
            else
            {
                buf[j++]=str[i++];
            }
        }

        buf[j]= '\0';
                
        if (target==cur_screen) 
                string_print(buf, pos, att);
        else
                virtual_string_print(target, buf, pos, att);
                
        pos += j*2;

        if(pos >= 80*25*2) 
        {
            (*nl)++;
            pos = 80*24*2;
            if (target==cur_screen) 
                scroll_up_one();
            else
                virtual_scroll_up_one(target);                  
        }
    }

    if (target==cur_screen) mouse_print();

    return pos;
}

void virtual_string_print(int target, char *buf, int pos, int att) 
{
    char *vbuf;
    int i;

    vbuf = t[target].screen_buf;

    i=0;
    while (buf[i]) 
    {
        vbuf[pos++] = buf[i++];
        vbuf[pos++] = (char) att;
    }
}

void virtual_scroll_up_one(int target) 
{
    int i;
    char *vbuf;

    vbuf = t[target].screen_buf;

    for(i=0; i<80*24*2; i++) 
    {
            vbuf[i] = vbuf[i+80*2];
    }

    i=80*24*2;
    while(i<80*25*2) 
    {
            vbuf[i++] = ' ';
            vbuf[i++] = 0x07;
    }
}

void signal(char c, char mask)
{
    CPOSITION it;
    struct key_suscription *s;
    struct csl_key_signal smsg;
    int i;

    // see if anyone is suscribed for this char signaling   
    // on current screen
    it = get_head_position(&t[cur_screen].suscribers);

    smsg.command = CSL_SIGNAL ;
    smsg.console = cur_screen;
    smsg.key = c;
    smsg.mask = mask;

    while(it != NULL)
    {
        s = (struct key_suscription *)get_next(&it);

        i = 0;
        while(i < s->susc)
        {
                if(s->keycodes[i] == c)
                        send_msg(s->taskid, s->port, &smsg);
                        
                i++;
        }
    }       
}

int check_ownership(int term, int task)
{
    int i = 0;

    /*while(i < t[term].owners)
    {
            if(t[term].ownerid[i] == task) return 1;
            i++;
    }
    return 0;*/
    return 1;
}
