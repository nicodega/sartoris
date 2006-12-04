/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/


#ifndef PMANPRINTH
#define PMANPRINTH

void pman_print(char *str, ...);
void pman_print_and_stop(char *str, ...);
void pman_print_clr(int next_color);
void pman_print_set_color(int c);
void pman_print_dbg(char *str, ...);
#endif
