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

#ifndef LINKEDPATTREEH
#define LINKEDPATTREEH

#include <lib/malloc.h>
#include <lib/structures/string.h>
#include <lib/structures/list.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remember to update lget_index if CHARS is modified! */
#define CHAR_COUNT 73
#define CHARS  {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','-','_','.','[',']','?','¿','!','¡','/','0','1','2','3','4','5','6','7','8','9','\0'};

#define NOT_OVERWITE

struct _lpointer
{
    int flags;     // xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxx|node
    void* content; // content might me an lnode or a pat-tree
};

struct _lpat_tree
{
    int diff;
    int pointer_count;
    char* prefix;
    struct _lpointer* pointers[CHAR_COUNT];
};

struct _lnode
{
    char* key;
    void* value;
    struct _lnode *next;
    struct _lnode *prev;
};

struct _ptlist
{
    struct _lpat_tree *pt;
    struct _lnode *first;
};

typedef struct _lnode lnode;
typedef struct _lpat_tree* ipat_tree;
typedef struct _ptlist lpat_tree;
typedef struct _lpointer lpointer;

int lpt_insert(lpat_tree *lpt, char* key, void* value);
int lpt_remove(lpat_tree *lpt, char *key);
void *lpt_getvalue(lpat_tree lpt, char *key);
void *lpt_getvalue_parcial_match(lpat_tree lpt, char *key);
void *lpt_getvalue_parcial_matchE(lpat_tree lpt, char *key, char **matched);
list lpt_getkeys(lpat_tree lpt);
list lpt_copy_getkeys(lpat_tree lpt);
list lpt_getvalues(lpat_tree lpt);
void lpt_init(lpat_tree *lpt);
int lpt_is_valid(char c);


#ifdef DRAW
    #include "stdio.h"
    void lpt_draw(lpat_tree lpt);
#endif

#ifdef __cplusplus
}
#endif

#endif
