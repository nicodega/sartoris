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

This data will be left only for executables requiring dynamic loading.

*/


struct init_data_dl
{
    unsigned int prg_start;     /* entry point for the task */
    unsigned int ld_start;      /* Base address for LD */
    unsigned int ld_dynamic;    /* Location of the dynamic section for LD */
    unsigned int ld_size;       /* size of LD memory image (all sections) */
    int          phsmo;         /* SMO to the program headers for the process */
    int          phsize;        /* Program header size */
    int          phcount;       /* Program headers count */
    void (*ldexit)(int ret);    /* If this value is 1 then on exit the process will 
                                call this function, which must be completed by LD. */
    unsigned int bss_end;		/* This will be the virtual address where defined segments end */
	unsigned int curr_limit;	/* Current segment limit */
    unsigned int creator_task;  /* This will contain the creator task */
    unsigned int param;         /* An integer parameter sent by the creator task */    
};

