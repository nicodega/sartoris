/*  
 *   Sartoris 0.5 kernel screen printig subsystem header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@gmail.com
 */

#ifndef PRINT_H
#define PRINT_H

typedef unsigned char byte;

void k_scr_init(void);
void k_scr_newLine(void);
void k_scr_moveUp(void);
void k_scr_print(char* text, byte att); 
void k_scr_hex(int n, byte att);
void k_scr_clear(void);
int kprintf(unsigned char att, char *format, ...);

#endif




