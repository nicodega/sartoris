/*
 *   Oblivion 0.2 ram filesystem service internals header
 *   basic FAT file system v0.2
 *
 *   Copyright (C) 2002, Santiago Bazerque, Nicolas de Galarreta and
 *   Matias Brustein Macri
 *
 *   sbazerqu@dc.uba.ar
 *   nicodega@cvtci.com.ar
 *   mbmacri@dc.uba.ar
 */

#ifndef _RAMFS_INT_
#define _RAMFS_INT_

#define CODE_SIZE 0x800
#define MAX_FILES 256;  


int get_filenum(char *s);
void acknowledge(int error_code, int task_id, struct fs_command *io_msg);
int streq(char *s, char *t);

#endif
