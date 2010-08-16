/*  
*       Text Mode VGA driver header
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*    modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

void clear_screen(void);
void scroll_up_one();
void scroll_up(int n);
void string_print(char *s, int pos, int att);
void cursor_on();
void cursor_off();
void set_cursor_pos(unsigned int pos);
void read_screen(char *buf);
void write_screen(char *buf);
