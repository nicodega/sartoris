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
*	Environment variables handling functions
*
*
*/

#include "shell_iternals.h"

void init_environment()
{
  int i = 0;  
  char *val;

  while(i <= NUM_TERMS)
  { 
	init(&vars[i]); 
	i++; 
  }

  val = (char*)malloc(64);
  val[0] = '/';
  val[1] = 'b';
  val[2] = 'i';
  val[3] = 'n';
  val[4] = '\0';

  // init global var PATH	
  set_global_env("PATH", val);
  
  // create CURRENT_PATH var on terms with the root path
  i = 0;
  while(i < NUM_TERMS)
  { 
	// set curent path
	val = (char*)malloc(64);
	val[0] = '/';
	val[1] = '\0';
	set_env("CURRENT_PATH", val, i) ; 
	// set term
	val = (char*)malloc(len(tty[i]) + 1);
	istrcopy(tty[i], val, 0);
	set_env("TERM", val, i) ; 
	i++; 
  }
  
}

int set_global_env(char *var_name, char *value)
{
	return set_env(var_name, value, NUM_TERMS);
}

int set_env(char *var_name, char *value, int term)
{
	struct env_var_value *var = NULL;	
	CPOSITION it;

	it = get_head_position(&vars[term]);

	while(it != NULL)
	{
		var = (struct env_var_value*)get_next(&it);
		if(streq(var->name, var_name))
			break;
	}
	
	if((var != NULL && !streq(var->name, var_name)) || var == NULL) 
	{
		var = (struct env_var_value *)malloc(sizeof(struct env_var_value));
		
		var->name = (char*)malloc(len(var_name) + 1);
		istrcopy(var_name, var->name, 0);
		add_head(&vars[term], var);
	}

	var->value = value;
}

char *get_global_env(char *var_name){ return get_env(var_name, NUM_TERMS); }

char *get_env(char *var_name, int term)
{
	struct env_var_value *var = NULL;
	CPOSITION it;

	it = get_head_position(&vars[term]);

	while(it != NULL)
	{
		var = (struct env_var_value*)get_next(&it);
		if(streq(var->name, var_name))
			break;
	}
	
	if((var != NULL && !streq(var->name, var_name)) || var == NULL) return NULL;

	return var->value;
}

void del_global_env(char *var_name){ return del_env(var_name, NUM_TERMS); }

void del_env(char *var_name, int term)
{
	struct env_var_value *var = NULL;
	CPOSITION it;

	it = get_head_position(&vars[term]);

	while(it != NULL)
	{
		var = (struct env_var_value*)get_next(&it);
		if(streq(var->name, var_name))
			break;
	}
	
	if((var != NULL && !streq(var->name, var_name)) || var == NULL)  return;
	
	remove_at(&vars[term], it);
	free(var->name);
	free(var);
}

int exists_env(char *var_name, int term)
{
	struct env_var_value *var = NULL;
	CPOSITION it;

	it = get_head_position(&vars[term]);

	while(it != NULL)
	{
		var = (struct env_var_value*)get_next(&it);
		if(streq(var->name, var_name))
			return 1;
	}
	
	return 0;
}

/* This function will process enviroment variable requests */
void process_env_msg()
{
	struct shell_cmd shellcmd;
	struct shell_res res;
	int count = get_msg_count(SHELL_PORT), id, vsize, nsize;
	char *name, *value;

	while(count > 0)
	{
		get_msg(SHELL_PORT, &shellcmd, &id);

		res.command = shellcmd.command;
		res.msg_id = shellcmd.msg_id;
		res.ret = SHELLERR_INVALIDCOMMAND;

        nsize = mem_size(shellcmd.name_smo);
		
		if(nsize <= 1)
		{
			res.ret = SHELLERR_INVALIDNAME;
			send_msg(id, shellcmd.ret_port, &res);
			count--;
			continue;
		}		

        if(shellcmd.value_smo != -1)
        {
            vsize = mem_size(shellcmd.value_smo);
		    if(vsize <= 1)
		    {
			    res.ret = SHELLERR_INVALIDVALUE;
			    send_msg(id, shellcmd.ret_port, &res);
			    count--;
			    continue;
		    }
        }
        else
        {
            vsize = -1;
        }

		switch(shellcmd.command)
		{
			case SHELL_GETENV:
				name = shell_get_string(shellcmd.name_smo);

				value = get_env(name, get_proc_term(id));
				if(value == NULL)
				{
					value = get_global_env(name);
				}
				
				if(value == NULL)
				{
					res.ret = SHELLERR_VAR_NOTDEFINED;
					break;
				}
                else if(value == ENV_EMPTY_VALUE)
				{
					if(vsize == 0)
				    {
					    res.ret = SHELLERR_SMO_TOOSMALL;
					    break;
				    }
				    if(write_mem(shellcmd.value_smo, 0, 1, "\0"))
				    {
					    res.ret = SHELLERR_SMOERROR;
					    break;
				    }
				}
                else
                {
				    if(vsize <= len(value))
				    {
					    res.ret = SHELLERR_SMO_TOOSMALL;
					    break;
				    }
				    if(write_mem(shellcmd.value_smo, 0, len(value) + 1, value))
				    {
					    res.ret = SHELLERR_SMOERROR;
					    break;
				    }
                }
				res.ret = SHELLERR_OK;
				break;
            case SHELL_ENVEXISTS:
                name = shell_get_string(shellcmd.name_smo);

				value = get_env(name, get_proc_term(id));
				if(value == NULL)
				{
					value = get_global_env(name);
				}
				
				if(value == NULL)
				{
					res.ret = SHELLERR_VAR_NOTDEFINED;
					break;
				}
				res.ret = SHELLERR_OK;
				break;
			case SHELL_SETENV:
				name = shell_get_string(shellcmd.name_smo);

                if(vsize == -1)
                    value == NULL;
                else
                {
				    value = (char*)malloc(vsize);
				    if(read_mem(shellcmd.value_smo, 0, vsize, value))
				    {
					    res.ret = SHELLERR_SMOERROR;
					    break;
				    }
				    if(value[vsize] != '\0')
				    {
					    res.ret = SHELLERR_INVALIDVALUE;
					    break;
				    }
                }
				if(shellcmd.global)
				{
					set_global_env(name, value);
				}
				else
				{
					set_env(name, value, get_proc_term(id));
				}
				res.ret = SHELLERR_OK;
				break;
		}

		send_msg(id, shellcmd.ret_port, &res);

		count--;
	}
}

char *shell_get_string(int smo)
{
	//get_string: gets a string from a Shared Memory Object
	int size = mem_size(smo);

	char *tmp = (char *)malloc(size);

	if(read_mem(smo, 0, size, tmp))
	{
		return NULL;		
	}

	return tmp;
}

int get_proc_term(int task)
{
	// find console assigned to the process, if none is found, return NUM_TERMS
	int i = 0;

	while(i < NUM_TERMS)
	{
		if(cslown[i].mode == SHELL_CSLMODE_PROC && cslown[i].process == task)
			break;
		i++;
	}

	return i;
}
