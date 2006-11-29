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

#include <services/pipes/pipes.h>
#include <sartoris/kernel.h>
#include <lib/const.h>
#include <lib/structures/avltree.h>
#include <lib/malloc.h>
#include <lib/scheduler.h>
#include <sartoris/syscall.h>
#include <os/pman_task.h>
#include <ints.h>
#include <lib/structures/list.h>
#include <services/stds/stdfss.h>
#include <services/stds/stdservice.h>
#include <lib/rawiolib.h>

#include <lib/debug.h>

extern AvlTree pipes;

#define PIPES_BUFFER_SIZE	1024 // 1kb buffer... it would be better if it was 4kb but we are short on space
#define PIPES_PORT		2

/* PIPE TYPES */
#define PIPES_FILEPIPE		1
#define PIPES_SHAREDPIPE	2

#define MIN(a,b) ((a > b)? b : a)
#define MAX(a,b) ((a > b)? a : b)

struct pipe_buffer;

struct pipe
{
	unsigned short id;	// pipe id
	unsigned short type;	// pype type
	unsigned int taskid;
	unsigned int taskid2;	
	FILE *pf;
	unsigned int creating_task;
	struct pipe_buffer *buffer;	// if its a shared pipe, this will hold the pipe buffer.
	int task1_closed;
	int task2_closed;
	struct pipes_cmd pending_cmd;
	int pending;
};

struct pipe_buffer
{
	unsigned long rcursor;	// cursor possition
	unsigned long wcursor;	// cursor possition
	unsigned long size;
	list blocks;
};

struct pipe_buffer_block
{
	unsigned char bytes[PIPES_BUFFER_SIZE];
};

/* Functions */
void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
void process_pipes_cmd(struct pipes_cmd *pcmd, int task);

struct pipes_res *build_response_msg(int ret);
unsigned short get_new_pipeid();
char *get_string(int smo);

struct pipes_res * pclose(struct pipes_close *pcmd, struct pipe *p, int task);
struct pipes_res * pread(struct pipes_read *pcmd, struct pipe *p);
struct pipes_res * pwrite(struct pipes_write *pcmd, struct pipe *p);
struct pipes_res * pseek(struct pipes_seek *pcmd, struct pipe *p, int task);
struct pipes_res * ptell(struct pipes_tell *pcmd, struct pipe *p, int task);
struct pipes_res * pputs(struct pipes_puts *pcmd, struct pipe *p);
struct pipes_res * pputc(struct pipes_putc *pcmd, struct pipe *p);
struct pipes_res * pgets(struct pipes_gets *pcmd, struct pipe *p);
struct pipes_res * pgetc(struct pipes_getc *pcmd, struct pipe *p);

struct pipe_buffer_block *alloc_block();
void free_block(struct pipe_buffer_block *);

struct pipes_res * preadgen(struct pipes_read *cmd, int delimited, struct pipe *p);
struct pipes_res * pwritegen(struct pipes_write *cmd, int delimited, struct pipe *p);

void process_pending(struct pipe *p);

