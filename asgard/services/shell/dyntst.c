/*
*	Shell Service.
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

/*
*
*	This command is only ment to test sartoris dynamic memory.
*
*/


#include "shell_iternals.h"

#include <sartoris/kernel.h>

void dyntst(int term, char **args, int argc)
{
    struct task mk_task;

    term_color_print(term, "\nCreating task MAX_TSK-1...  ", 7);
    
    mk_task.mem_adr = (void*)0xC800000;
	mk_task.size = 0x1000;
	mk_task.priv_level = 0;

	if(create_task(MAX_TSK-1, &mk_task))
    {
		term_color_print(term, "FAILED\n", 12);
		return;
    }

    term_color_print(term, "OK\n", 11);
    term_color_print(term, "\nDestroying task MAX_TSK-1...  ", 7);

    if(destroy_task(MAX_TSK-1))
    {
		term_color_print(term, "FAILED\n", 12);
		return;
    }
    term_color_print(term, "OK\n", 11);

    return;
}

