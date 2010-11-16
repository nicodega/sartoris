/*
*   Oblivion ram filesystem service
*   
*
*   Copyright (C) 2002, Santiago Bazerque, Nicolas de Galarreta
*
*   sbazerqu@dc.uba.ar
*   nicodega@cvtci.com.ar
*/

/*   basic FAT file system v0.2 support by
*   Matias Brunstein Macri
*   
*   mbmacri@dc.uba.ar
*/


#include <sartoris/syscall.h>
#include <oblivion/layout.h>
#include <services/ramfs/ramfs.h>
#include <services/filesys/filesys.h>
#include <lib/bfilesys.h>
#include "ramfs_internals.h"

int nfiles;
struct fatline  fatable[256];

char name[50];

const void *ramfiledata = (void*) CODE_SIZE;

void ramfs(void) {
    struct fs_command io_msg;
    int id;
    int filenum;

    __asm__ ("sti" : :);

    open_port(FS_CMD_PORT, 0, UNRESTRICTED);

    readfat((void*)ramfiledata, &nfiles, fatable);

    for (;;) {
        while (get_msg_count(FS_CMD_PORT)>0) {
            get_msg(FS_CMD_PORT, &io_msg, &id);

            switch(io_msg.op) 
            {
              case FS_READ:
                  if (read_mem(io_msg.smo_name, 0, FILENAME_SIZE/4, name) < 0) 
                  {
                      acknowledge(FS_FAIL, id, &io_msg);
                  } else {
                      filenum = get_filenum(name);
                      if (filenum < 0) {
                          acknowledge(FS_NO_SUCH_FILE, id, &io_msg);
                      } else {
                          if (write_mem(io_msg.smo_buff, 0,
                              fatable[filenum].filesize / 4 + 
                              ((fatable[filenum].filesize % 4)?1:0),
                              fatable[filenum].filepos) < 0) {
                                  acknowledge(FS_FAIL, id, &io_msg);
                          } else {
                              acknowledge(FS_OK, id, &io_msg);
                          }
                      }
                  }
          break;
            }
        }
        run_thread(SCHED_THR);
    }
}

int get_filenum(char *s) {
    int i;

    for (i=0; i<nfiles; i++) {
        if (streq(s, fatable[i].filename)) return i;
    }

    return -1;
}

void acknowledge(int error_code, int task_id, struct fs_command *io_msg) {
    struct fs_response res_msg;

    res_msg.op = io_msg->op;
    res_msg.id = io_msg->id;
    res_msg.result = error_code;
    res_msg.smo_name = io_msg->smo_name;
    res_msg.smo_buff = io_msg->smo_buff;

    send_msg(task_id, io_msg->ret_port, &res_msg);
}

int streq(char *s, char *t) {
    int i=0;
    char c;

    while((c=s[i]) == t[i]) {
        if (!c) return 1;
        i++;
    }

    return 0;
}
