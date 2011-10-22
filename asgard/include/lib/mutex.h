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

#ifndef MUTEXH
#define MUTEXH

#define TURN_MASK (unsigned int)0x0000FFFF

#ifdef __cplusplus
extern "C" {
#endif

struct mutex
{
    unsigned int available_turn;  // this holds last taken turn 
    unsigned int locking_turn;    // holds the turn of the locking thread         
    int locking_thread;
    int recursion;
    int lock;
};


void init_mutex(struct mutex *mon);
void wait_mutex(struct mutex *mon);
void leave_mutex(struct mutex *mon);
void close_mutex(struct mutex *mon);
int test_mutex(struct mutex *mon);
int own_mutex(struct mutex *mon);

#ifdef __cplusplus
}
#endif

#endif
