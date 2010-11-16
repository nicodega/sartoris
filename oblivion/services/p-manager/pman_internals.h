/*  
 *   Oblivion process manager internals header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */

#ifndef _PMAN_H_
#define _PMAN_H_

#include <sartoris/kernel.h>

#define SCHED_HERTZ 200

#define NAME_LENGTH 20
#define INPUT_LENGTH 80

#define RAMFS_INPUT_PORT 3
#define FILESYS_INPUT_PORT 2
#define TERM_REQ_PORT 4
#define RUNNING_PORT 4

#define PROMPT 0
#define LOADING 1
#define INITIALIZING 2
#define RUNNING 3
#define KILLED 4

#define NUM_SERV_THR 32
#define NUM_PROC 8

#define BASE_PROC_TSK 16
#define BASE_PROC_THR (MAX_THR-8)

#define SARTORIS_PROC_BASE_LINEAR   MIN_TASK_OFFSET

void spawn_handlers(void);
void handler(void);

void console_begin(int term);

void show_prompt(int term);
void get_input(int term);

void fetch_file(int term, int fs_task, int ret_port);
void do_load(int term);
void do_unload(int term);
void inform_load_error(int term, int status);

int prefix(char *p, char *s);
int streq(char *s, char *t);

int csl_print(int csl, char *str, int att, int len, int res_code);

#define STOP for(;;){}

#endif
