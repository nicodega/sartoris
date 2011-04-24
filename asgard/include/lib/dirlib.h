/*
*
*    DIRLIB 0.1: A common library for using the directory service.
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*     modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#include <sartoris/syscall.h>
#include <lib/structures/string.h>
#include <lib/const.h>
#include <lib/malloc.h>
#include <services/directory/directory.h>
#include <lib/scheduler.h>
#include <os/pman_task.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Directory task is st on initfs2conf file under pmanager2 file tree. */
#define DIRLIB_PORT 5

#ifndef DIRLIBH
#define DIRLIBH

void dir_set_port(int port);
int dir_register(char *service_name);
int dir_unregister();
int dir_resolveid(char *service_name);
char *resolve_name(int serviceid);

#ifdef __cplusplus
}
#endif

#endif

