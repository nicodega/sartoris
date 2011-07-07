/*
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
#include <services/stds/stdfss.h>
#include <services/stds/stdservice.h>
#include <lib/const.h>
#include <services/directory/directory.h>
#include <lib/scheduler.h>
#include <os/pman_task.h>

#ifndef LDIOH
#define LDIOH

#define EOF -1
#define IOPORT 2

typedef struct SFILE
{
    int fs_serviceid;
    int file_id;
    int eof;
} FILE;

#define EXISTS_FILE   STDFSS_FILETYPE_FILE
#define EXISTS_DIR    STDFSS_FILETYPE_DIRECTORY

int fopen(char *filename, FILE *file);
int fclose(FILE *stream);
size_t fread(char *buffer, int bytes, FILE *stream);
int exists(char *filename, int type);

/* intenal */
int resolve_fs();
int send_fs(int task, int *msg, int *res);
int get_response(int task, int *res);

#endif

