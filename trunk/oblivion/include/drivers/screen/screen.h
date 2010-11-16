/*  
 *   Oblivion 0.1 VGA driver header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
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
