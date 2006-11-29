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
*	Shell set command
*
*/

#include "shell_iternals.h"

void set_cmd(int term, char **args, int argc)
{
	char *val = NULL, *oldval = NULL, *varname;
	int get = 0, tm;
	
	if(argc == 0)
	{
		// list variables
		list_env(term);
		return;
	}

	if(argc > 5 || argc < 3 || (!streq(args[argc - 2], "=") && !streq(args[argc - 2], "+=")))
	{
		term_color_print(term, "\nInvalid parameters.\n", 12);
		term_color_print(term, "usage: set [-r] [-g] [var {=|+=} \"value\"]\n", 7);
		return;	
	}

	varname = args[argc - 3];
	val = args[argc - 1];

	if(streq(varname, "CURRENT_PATH") || streq(varname, "TERM"))
	{
		term_color_print(term, "\nCannot change this variable.\n", 12);
		return;
	}
	
	if(streq(args[argc - 2], "+=")) get = 1;

	tm = term;

	// check for global var
	if((argc > 3 && streq(args[1], "-g")) || (argc == 5 && streq(args[2], "-g")))
	{
		tm = NUM_TERMS;
	}

	// check if variable is being deleted
	if((argc > 3 && streq(args[1], "-r")) || (argc == 5 && streq(args[2], "-r")))
	{
		del_env(varname, tm);
		return;
	}


	oldval = get_env(varname, tm);

	if(get && len(val) == 0) return;
	
	// if variable is not global, an append is being performed
	// and variable is not defined on the console, copy
	// current variable value from global
	if(get && oldval == NULL && tm != NUM_TERMS)
	{
		oldval = get_env(varname, NUM_TERMS);
		val = (char *)malloc(len(oldval) + len(val) + 1);
		istrcopy(oldval, val, 0);
		istrcopy(args[argc - 1], val, len(oldval));
		oldval = NULL; // avoid deletion
	}
	else if(get && oldval != NULL)
	{
		val = (char *)malloc(len(oldval) + len(val) + 1);
		istrcopy(oldval, val, 0);
		istrcopy(args[argc - 1], val, len(oldval));
	}
	else
	{
		val = (char *)malloc(len(val) + 1);
		istrcopy(args[argc - 1], val, 0);
	}

	if(oldval != NULL) free(oldval);

	set_env(varname, val, tm);
}

void list_env(int term)
{
	CPOSITION it = NULL;
	char *key;
	struct env_var_value *val;

	// show globals
	
	it = get_head_position(&vars[NUM_TERMS]);

	while(it != NULL)
	{
		val = (struct env_var_value *)get_next(&it);

		// ignore globals when locally defined
		if(!exists_env(val->name, term))				
		{
			term_color_print(term, val->name, 7);
			term_color_print(term, " = \"", 7);
			term_color_print(term, val->value, 7);		
			term_color_print(term, "\"\n", 7);
		}
	}

	// show console locals
	it = get_head_position(&vars[term]);

	while(it != NULL)
	{
		val = (struct env_var_value *)get_next(&it);
		
		term_color_print(term, val->name, 7);
		term_color_print(term, " = \"", 7);
		term_color_print(term, val->value, 7);		
		term_color_print(term, "\"\n", 7);
	}
}

