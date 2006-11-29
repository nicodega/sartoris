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
*	Mount and umount utilities
*
*/

#include "shell_iternals.h"

// usage: mount dev_file path -rwd
void mount_cmd(int term, char **args, int argc)
{
	int mode = IOLIB_MOUNTMODE_READ | IOLIB_MOUNTMODE_WRITE | IOLIB_MOUNTMODE_DEDICATED;
	int dev_index = 0;

	if(argc != 2 && argc != 3)
	{
		term_color_print(term, "Invalid parameters.\n", 12);
		term_color_print(term, "Usage: mount [-r|w|d] device_file path.\n", 7);
		return;
	}

	if(argc == 3)
	{
		mode = 0;

		// calculate mount mode
		if(streq(args[0] , "-r"))
		{
			mode |= IOLIB_MOUNTMODE_READ;
		}
		else if(streq(args[0] , "-w"))
		{
			mode |= IOLIB_MOUNTMODE_WRITE;
		}
		else if(streq(args[0] , "-rd") || streq(args[0] , "-dr"))
		{
			mode |= IOLIB_MOUNTMODE_READ | IOLIB_MOUNTMODE_DEDICATED;
		}
		else if(streq(args[0] , "-wd") || streq(args[0] , "-dw"))
		{
			mode |= IOLIB_MOUNTMODE_WRITE | IOLIB_MOUNTMODE_DEDICATED;
		}
		else if(streq(args[0] , "-rwd") || streq(args[0] , "-rdw") || streq(args[0] , "-drw")  || streq(args[0] , "-dwr")  || streq(args[0] , "-wrd"))
		{
			mode |= IOLIB_MOUNTMODE_WRITE | IOLIB_MOUNTMODE_READ | IOLIB_MOUNTMODE_DEDICATED;
		}
		else if(streq(args[0] , "-wr") || streq(args[0] , "-rw"))
		{
			mode |= IOLIB_MOUNTMODE_WRITE | IOLIB_MOUNTMODE_READ;
		}
		else
		{
			term_color_print(term, "Mount mode is invalid.\n", 12);
			term_color_print(term, "Usage: mount [-r|w|d] device_file path.\n", 7);
			return;
		}
		dev_index = 1;
	}

	if(mount(args[dev_index], args[dev_index+1], mode))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 12);
	}
}

void umount_cmd(int term, char **args, int argc)
{
	char *current_dir = NULL;

	if(argc != 1)
	{
		term_color_print(term, "\nInvalid parameters.\n", 12);
		term_color_print(term, "Usage: umount path\n", 7);
		return;
	}

	current_dir = get_env("CURRENT_PATH", term);
	
	if(strprefix(args[0], current_dir))
	{
		term_color_print(term, "Cannot umount dir, because it's a prefix of the current path.\n", 12);
		return;
	}

	if(umount(args[0]))
	{
		term_color_print(term, mapioerr(ioerror()), 12);
		term_color_print(term, "\n", 12);
	}
}
