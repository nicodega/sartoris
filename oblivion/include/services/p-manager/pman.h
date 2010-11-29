/*  
 *   Oblivion 0.1 floppy filesystem header
 *   
 *   Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
 *   
 *   sbazerqu@dc.uba.ar                
 *   nicodega@cvtci.com.ar
 */



#ifndef PMANM
#define PMANM

////////////////////////////////
//       PMAN Messages        //
////////////////////////////////

#define PMAN_CMD_INIT  0
#define PMAN_CMD_PERMS 1    // message used to require thread execute permissions from pman
#define PMAN_CMD_MPERMS 2   // message used to require port permissions from pman

struct pman_msg
{
    int cmd;
    int order;
    int param1;
    int param2;
} __attribute__ ((__packed__));

struct pman_res
{
    int cmd;
    int order;
    int param1;
    int param2;
} __attribute__ ((__packed__));

#endif
