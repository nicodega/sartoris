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

/* the following structure will be left by the process manager 
upon loading, on the stack space, but sp will be pointing at the begining 
and not the end of this struct, that's why init must setup the stack properly.
process manager will leave the stack in this way: 

		 --------------------------
		| init struct (size bytes) | 
		|                          | <-- esp (at the end of init struct, which will be upside down in memory)
		|--------------------------|
		| init struct size		   | 
		 --------------------------
		
*/
struct init_data
{
	unsigned int bss_end;		/* This will be the virtual address where defined segments end */
	unsigned int curr_limit;	/* Current segment limit */
    unsigned int creator_task;  /* This will contain the creator task */
    unsigned int param;         /* An integer parameter sent by the creator task */
};

