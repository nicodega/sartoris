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
#include <lib/cppabi.h>

extern int io_consoleid;

extern int main(int, char**); // on cpp main uses C linkage

void _exit(int) __attribute__ ((noreturn));

extern unsigned int __end_dtors;
extern unsigned int __start_dtors;
extern unsigned int __start_ctors;
extern unsigned int __end_ctors;

struct atexit_func atexit_funcs[_ATEXIT_MAXFUNCS];
unsigned int atexit_count = 0;
void *__dso_handle = 0; 

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

    open_port(STDPROCESS_DIRLIBPORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDPROCESS_IOPORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDPROCESS_PORT, 2, PRIV_LEVEL_ONLY);
    
	// init mem
	init_mem((void*)initd->bss_end, initd->curr_limit);

	dir_set_port(STDPROCESS_DIRLIBPORT);

	// init iolib
	set_ioports(STDPROCESS_IOPORT);
    initio();

    // send a message to the shell, asking for the init msg
    send_msg(initd->creator_task, initd->param, &res);
        
    // now wait for the init cmd
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
    unsigned int *ctor = &__start_ctors;
	for(; ctor < &__end_ctors; ++ctor)
    {
        ((void (*) (void)) (*ctor)) ();
    }

  	// call main function
	int ret = main(argc, args);

	// invoke static C++ destructors (on reverse order)
    if(__end_ctors != __start_ctors)
    {
        unsigned int *dtor = &__end_ctors;
        dtor--;
	    for(;dtor >= &__start_ctors; dtor--)
            ((void (*) (void)) (*dtor)) ();
    }
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
 
int __cxa_atexit(void (*f)(void *), void *ptr, void *dso)
{
    print("proc init: atexit\n");
	if (atexit_count >= _ATEXIT_MAXFUNCS) {return -1;};
	atexit_funcs[atexit_count].dtor = f;
	atexit_funcs[atexit_count].ptr = ptr;
	atexit_funcs[atexit_count].dso_handle = dso;
	atexit_count++;
    print("proc init: atexit ok\n");
	return 0;
}
 
void __cxa_finalize(void *f)
{
	unsigned int i = atexit_count;
	if (!f)
	{
		/*
		* Destroy everything, since f is NULL (we will destroy them on the reverse order)
		*/
        while (--i)
		{
			if (atexit_funcs[i].dtor)
				(*atexit_funcs[i].dtor)(atexit_funcs[i].ptr);
		}
		return;
	};
 
	for ( ; i >= 0; )
	{
		if (atexit_funcs[i].dtor == f)
		{
			(*atexit_funcs[i].dtor)(atexit_funcs[i].ptr);
			atexit_funcs[i].dtor = 0;
 		}
	}
}

