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

#define PMAN_CMD_PORT 6

#define PMAN_CMD_DEBUG 1
#define PMAN_CMD_DEBUG_END 2


#define PMAN_EVT_BREAK 3

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
