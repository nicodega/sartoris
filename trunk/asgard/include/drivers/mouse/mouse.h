/*
*
*    Copyright (C) 2002 - 2011
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

#ifndef MOUSEH
#define MOUSEH

extern char mouse_int_handler;
extern char mouse_deltax;
extern char mouse_deltay;
extern char state;
extern char estate;
extern char mouse_changed;

void mouse_init(char resolution, char scaling, char sampling_rate);

#endif
