/*  
 *   Sartoris 0.5 i386 kernel screen driver
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

/* 
 * used to print dumps, critical messages, etc
 */


/* this code uses the flat-mode descriptors only */

#include "sartoris/scr-print.h"

#define TEXT_ADDRESS 0xB8000    

extern void conf_vga(void);

char* buff;

void k_scr_init()
{
  conf_vga();
}

void k_scr_moveUp()
{
  int i = 0;

  /* copio la memoria para arriba y a Buff lo pongo a una linea del final */
  
  buff = (char*)TEXT_ADDRESS;  /* me posiciono al comienzo del buffer */
  
  

  while (i < 4000)   /* proceso 24 lineas */
  {
    *buff = *(buff + 160);        /* copio a los de las otras posiciones */
    buff++;
    i++;
  }

  buff -= 160;

  return;
}

void k_scr_newLine()
{
  buff = ((buff + 160) - (((long)buff - 0xB8000) % 160));    /* avanzo 160 bytes y le resto la pos actual */
  
  if ((long)buff >= 0xb8fa0) { k_scr_moveUp(); }
  return;
}


void k_scr_print(char *text, unsigned char att)
{
  int i = 0;

  while (text[i] != '\0')
  { 
    if (text[i] == '\n') 
    {
      k_scr_newLine();
      i++;
    }
    else
    {
     *buff = text[i];
     buff++;

     *buff = att;
     buff++;

     if ((long)buff >= 0xB8fa0){ k_scr_moveUp(); }
     
     i++;
    }
  }
  
  return;
}

void k_scr_hex(int n, unsigned char att) {
  int i;
  unsigned char c;
  unsigned char t[8];
  
  for(i=7; i>=0; i--) {
    c = (unsigned char) (n & 0xf);
    if (c > 9) { c = c + 'a' - 0xa; } else { c = c + '0'; }
    t[i] = c;
    n = n >> 4;
  }
  
  for(i=0; i<8; i++) {
    *buff = t[i];
    buff++;
    *buff = att;
    buff++;
  }
  
  if ((long)buff >= 0xB8fa0) { k_scr_moveUp(); }
    
}

void k_scr_clear()
{
  int i = 0;
  buff = (char*)TEXT_ADDRESS;
  
  while (i < 4000)    /* cambio todo */
  {
    *buff = ' ';
    buff++;
    *buff = 0;
    buff++;
    i = i + 2;
  }
  
  buff = (char*)TEXT_ADDRESS;

  return;
}





