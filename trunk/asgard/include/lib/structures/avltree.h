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

#include <lib/const.h>


#ifndef AVLTREE_H
#define AVLTREE_H

/*
*  Which of a given node's subtrees is higher?
*/
enum AVLSKEW
{
	NONE,
	LEFT,
	RIGHT
};

/*
*  Did a given insertion/deletion succeed, and what do we do next?
*/
enum AVLRES
{
	AVLERROR = 0,
	OK,
	BALANCE,
};

/*
*  AVL tree node structure
*/
struct avlnode
{
	struct avlnode *left, *right;
	void *d;
	int key;
	enum AVLSKEW skew;
};

struct savl_tree
{
	struct avlnode *node;
	int total;
};

typedef struct savl_tree AvlTree;

void avl_init(AvlTree *avl);

enum AVLRES avl_insert(AvlTree *n, void *d, int key);

enum AVLRES avl_remove(AvlTree *n, int key);

void *avl_getvalue(AvlTree n, int key);
int avl_get_indexes(AvlTree *n, int **indexes);
int avl_get_total(AvlTree *n);
void avl_remove_all(AvlTree *n);

#endif


