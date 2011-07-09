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

#include <lib/iolib.h>
#include <lib/signals.h>
#include <os/pman_task.h>
#include <services/pmanager/signals.h>

int main(int argc, char **argv) 
{
	int i = 0;
	char buff[50];

	printf("Blocking sleep signal for 5 seconds\n");

	if(wait_signal(PMAN_SLEEP, PMAN_TASK, 5000, PMAN_SIGNAL_PARAM_IGNORE, PMAN_SIGNAL_PARAM_IGNORE, NULL, NULL))
	{
		printf("timed out\n");
	}
	else
	{
		printf("Awake again\n");
	}

	printf("Non Blocking sleep signal for 5 seconds\n");

	SIGNALHANDLER sh = wait_signal_async(PMAN_SLEEP, PMAN_TASK, 5000, PMAN_SIGNAL_PARAM_IGNORE, PMAN_SIGNAL_PARAM_IGNORE);
	
	while( check_signal(sh, NULL, NULL) == 0);

	printf("back again!\n");

	discard_signal(sh);

	printf("Non Blocking sleep signal for 60 seconds\n");

	sh = wait_signal_async(PMAN_SLEEP, PMAN_TASK, 60000, PMAN_SIGNAL_PARAM_IGNORE, PMAN_SIGNAL_PARAM_IGNORE);
	
	printf("still running discarding signal!\n");

	discard_signal(sh);
	
	return 0;
} 
	
