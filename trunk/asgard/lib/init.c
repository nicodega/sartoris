/*
*	This implements the standard process interface for getting arguments from the shell, 
*	and setting up a process.
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

#include <sartoris/syscall.h>
#include <sartoris/kernel.h>
#include <os/pman_task.h>
#include <lib/scheduler.h>
#include <lib/printf.h>
#include <lib/parameters.h>
#include <proc/stdprocess.h>
#include <lib/malloc.h>
#include <lib/const.h>
#include <lib/structures/string.h>
#include <lib/iolib.h>
#include <proc/init_data.h>
#include <services/pmanager/services.h>

extern int io_consoleid;

char *tty[8] = {	"/dev/tty0",
			"/dev/tty1",
			"/dev/tty2",
			"/dev/tty3",
			"/dev/tty4",
			"/dev/tty5",
			"/dev/tty6",
			"/dev/tty7"};

extern int main(int, char**)  __attribute__ ((noreturn));
void _exit(int) __attribute__ ((noreturn));

extern void __end_dtors();
extern void __start_dtors();
extern void __start_ctors();
extern void __end_ctors();

/*
*
*	__procinit will perform the following tasks:
*		- initialize malloc
*		- initalize IO
*		- Get shell parametrs 
*
*/
void __procinit(struct init_data *initd) 
{
	struct stdprocess_init init_cmd;
	struct stdprocess_res res;
	int id;
	char **args = NULL, *ln;
	int argc = 0, s;
	
	res.command = STDPROCESS_INIT;
	res.ret = STDPROCESSERR_OK;

	// init mem
	init_mem((void*)initd->bss_end, initd->curr_limit);

	dir_set_port(STDPROCESS_DIRLIBPORT);

	// init iolib
	initio();
	set_ioports(STDPROCESSIOPORT);
  
	while(get_msg_count(STDPROCESS_PORT) == 0) { reschedule(); }

	// get msg from shell
	get_msg(STDPROCESS_PORT, &init_cmd, &id);

	// get console from the shell  
	io_consoleid = init_cmd.consoleid;
  
	// readmem
	s = mem_size(init_cmd.cl_smo);

	if(s > 0)
	{
		ln = (char*)malloc(s);

		if(read_mem(init_cmd.cl_smo, 0, s, ln))
		{
			_exit(-1);
		}
		
		// parse arguments from command line
		argc = get_param_count(ln);

		if(argc > 0)
		{
			args = (char**)malloc(argc * sizeof(char*));
			get_parameters(ln, argc, args);
		}
	}

	// see if stdxx mapping is required
	if(init_cmd.map_smo != -1)
	{
		struct map_params map_params;
		if(read_mem(init_cmd.map_smo, 0, sizeof(struct map_params), &map_params))
		{
			_exit(-1);
		}
		if(map_params.mapstdin) map_std(&stdin, &map_params.stdin);
		if(map_params.mapstdout) map_std(&stdout, &map_params.stdout);
		if(map_params.mapstderr) map_std(&stderr, &map_params.stderr);
	}

	// send a response to the shell
	send_msg(init_cmd.shell_task, init_cmd.ret_port, &res);

	// invoke static C++ constructors
	void (*ctor)() = __start_ctors;
	
	while((unsigned int)ctor != (unsigned int)__end_ctors)
	{ 
		ctor(); 
		ctor++;
	}

  	// call main function
	int ret = main(argc, args);

	// invoke static C++ destructors (on reverse order)
	void (*dtor)() = __end_dtors;

	dtor--;

	while((unsigned int)dtor != (unsigned int)__start_dtors)
	{ 
		dtor(); 
		dtor--;
	}
	dtor();

	_exit(ret);
}

void _exit(int ret) 
{
	struct pm_msg_finished finished;

	fclose(&stdin);
	fclose(&stdout);
	fclose(&stderr);

	close_malloc_mutex();

	finished.pm_type = PM_TASK_FINISHED;
	finished.req_id = 0;
	finished.ret_value = ret;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &finished);

	for(;;) { reschedule(); }
}
