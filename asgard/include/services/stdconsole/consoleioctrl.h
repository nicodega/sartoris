/*
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


#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#ifndef _CONSOLEIOCTRL_H_
#define _CONSOLEIOCTRL_H_

#define CSL_IO_SWITCH           1
#define CSL_IO_SETATT           2
#define CSL_IO_SETSIGNALPORT    3
#define CSL_IO_SETMSIGNALPORT   4
#define CSL_IO_SIGNAL           5
#define CSL_IO_USIGNAL          6
#define CSL_IO_DISABLE_ECHO     7 // this ioctl will override stdchar dev msgs echo
#define CSL_IO_ENABLE_ECHO      8 // this ioctl will override stdchar dev msgs echo
#define CSL_IO_COMMAND_ECHO     9
#define CSL_IO_MSIGNAL          10
#define CSL_IO_UMSIGNAL         11
#define CSL_MOUSE_ENABLE        12

/* This will be the message sent by the console when a singaled key is pressed */
#define CSL_SIGNAL              1
#define CSL_MOUSE_SIGNAL        2

struct csl_key_signal
{
    int command;
    char key;
    char mask;
    short padding0;
    int padding1;
    int console;
} PACKED_ATT;

struct csl_mouse_signal
{
    int command;
    char buttons;   // use CSL_BUTTON to see if it was pressed
    char wheel_mov; // -1 or 1 if the wheel moved, 0 otherwise
    char x;
    char y;
    int padding1;
    int console;
} PACKED_ATT;

#define CSL_LBUTTON     1
#define CSL_RBUTTON     2
#define CSL_MBUTTON     4

#endif

