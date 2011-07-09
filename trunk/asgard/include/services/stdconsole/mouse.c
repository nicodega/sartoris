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
#include <drivers/mouse/mouse.h>

extern int cur_screen;

int mouse_x = CONSOLE_COLS*MOUSE_PX_CHAR;
int mouse_y = CONSOLE_ROWS*MOUSE_PX_CHAR;

int mouse_enabled[NUM_VIRTUAL];

unsigned int mouse_col = CONSOLE_COLS;  // max 80
unsigned int mouse_row = CONSOLE_ROWS;  // max 25

char old_mouse_char = ' ';
int old_mouse_att = 0x7;

void init_mouse()
{
    int i;

    for(i = 0; i < NUM_VIRTUAL; i++)
        mouse_enabled[i] = 0;
        
    for(i = 0; i < MAX_MOUSE_SUSCRIPTIONS; i++)
        mouse_suscriptions[i] = -1;
    msuscs = 0;
    
    create_mouse_thread();
}

void create_mouse_thread(void) 
{
        struct pm_msg_create_thread msg_create_thr;
        struct pm_msg_response      msg_res;
        int port = 3, sender_id = 0, i;

        msg_create_thr.pm_type = PM_CREATE_THREAD;
        msg_create_thr.req_id = 0;
        msg_create_thr.response_port = port;
        msg_create_thr.task_id = get_current_task();
	    msg_create_thr.stack_addr = NULL;
        msg_create_thr.entry_point = &mouse_int_handler;
        msg_create_thr.interrupt = 44;      // int 12
        msg_create_thr.int_priority = 1;

        send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

        while (get_msg_count(port) == 0){reschedule();}

        get_msg(port, &msg_res, &sender_id);
    
    // initialize the mouse
    mouse_init(MOUSE_RESOLUTION, MOUSE_SCALING, MOUSE_SAMPLING_RATE);
}

void do_mouse()
{
    // see if the mouse has changed since we checked last
    if(mouse_changed)
    {
        if(mouse_enabled[cur_screen])
        {
            asm ( "cli" : : );
        
            mouse_changed = 0; // reset it
            char cmouse_int_handler = mouse_int_handler;
            char cmouse_deltax = mouse_deltax;
            char cmouse_deltay = mouse_deltay;
            char cstate = state;
            char cestate = estate;
        
            asm ( "sti" : : );
        
            if(cstate & 0xC0)
                return;
            
            int dx, dy;

            // get the deltas
            if(cstate & 0x10)
                dx = (int)(0xFFFFFF00 | cmouse_deltax);
            else
                dx = (int)(0x000000FF & cmouse_deltax);

            if(cstate & 0x20)
                dy = (int)(0xFFFFFF00 | cmouse_deltay);
            else
                dy = (int)(0x000000FF & cmouse_deltay);

            dy = -dy;

            if(mouse_x + dx < 0)
                mouse_x = 0;
            else if(mouse_x + dx >= MOUSE_MAXX)
                mouse_x = MOUSE_MAXX;
            else
                mouse_x += dx;

            if(mouse_y + dy < 0)
                mouse_y = 0;
            else if(mouse_y + dy >= MOUSE_MAXY)
                mouse_y = MOUSE_MAXY;
            else
                mouse_y += dy;

            unsigned int nmouse_col = (unsigned int)(mouse_x / MOUSE_PX_CHAR);
            unsigned int nmouse_row = (unsigned int)(mouse_y / MOUSE_PX_CHAR);
                        
            // where any buttons pressed?
            char buttons = (cstate & 0x7);
            char wheel_mov = (cestate & 0xF)? -1 : (cestate & 0x1);
            struct csl_mouse_signal sig;

            if((buttons || wheel_mov) && msuscs)
            {
                sig.command = CSL_MOUSE_SIGNAL;
                sig.buttons = buttons;
                sig.wheel_mov = wheel_mov;
                sig.x = nmouse_col;
                sig.y = nmouse_row;
                sig.console = cur_screen;
            
                int i;
                for(i = 0; i < MAX_MOUSE_SUSCRIPTIONS; i++)
                {
                    if(mouse_suscriptions[i] != -1)
                    {
                        send_msg(mouse_suscriptions[i], mouse_suscriptions_ports[i], &sig);
                    }
                }
            }

            // update the cursor position
            if(nmouse_col != mouse_col || nmouse_row != mouse_row)
            {           
                mouse_restore();

                mouse_col = nmouse_col;
                mouse_row = nmouse_row;

                mouse_print();
            }
        }
        else
        {
            asm ( "cli" : : );        
            mouse_changed = 0; // reset it
            asm ( "sti" : : );
        }
    }
}

void mouse_restore()
{
    if(mouse_enabled[cur_screen]) char_print(old_mouse_char, ((mouse_col + CONSOLE_COLS * mouse_row )<< 1), old_mouse_att);
}

void mouse_print()
{
    if(mouse_enabled[cur_screen])
    {
        char_read(&old_mouse_char, ((mouse_col + CONSOLE_COLS * mouse_row )<< 1), &old_mouse_att);
        char_print(219, ((mouse_col + CONSOLE_COLS * mouse_row )<< 1), 15);
    }
}

void mouse_reset_pos()
{
    mouse_restore();
    mouse_col = 0;
    mouse_row = 0;
    mouse_print();
}

void mouse_enable(int enabled)
{
    if(enabled)
    {
        if(mouse_enabled[cur_screen]) return;
        mouse_enabled[cur_screen] = 1;
        mouse_changed = 0;
        mouse_col = CONSOLE_COLS-1;
        mouse_row = CONSOLE_ROWS-1;
        mouse_print();
    }
    else
    {
        if(!mouse_enabled[cur_screen]) return;
        mouse_restore();
        mouse_enabled[cur_screen] = 0;        
    }
}
