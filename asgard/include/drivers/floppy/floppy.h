/*
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*     modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#ifndef FLOPPY
#define FLOPPY

void send_data(int d);
int get_data();
int on_command();
int busy();
int req();
void set_floppy(int flags);
void set_data_rate(int flags);
int disk_change();

#define DRIVE_ENABLE 4
#define DMA_ENABLE    8
#define MOTOR_ON 16

#endif
