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

#include <lib/libstest.h>
#include <lib/printf.h>

void main(int argc, char **argv)
{    
    if(static_int == 30)
        printf("static is 30\n");
    int r = shared_test(4);
    printf("r is %i static is %i", r, static_int);
    fgetc(&stdin);
}
