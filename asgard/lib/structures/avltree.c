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

#include <lib/structures/avltree.h>
#include <lib/malloc.h>

void avl_init(AvlTree *avl)
{
	avl->node = (struct avlnode*)NULL;
	avl->total = 0;
}


void avlrotleft(struct avlnode **n)
{
	struct avlnode *tmp = *n;

	if(n == NULL || *n == NULL) return;

	*n = (*n)->right;
	tmp->right = (*n)->left;
	(*n)->left = tmp;
}

void avlrotright(struct avlnode * *n)
{
	struct avlnode *tmp = *n;

	if(n == NULL || *n == NULL) return;

	*n = (*n)->left;
	tmp->left = (*n)->right;
	(*n)->right = tmp;
}


enum AVLRES avlleftgrown(struct avlnode **n)
{
	switch ((*n)->skew) {
	case LEFT:
		if ((*n)->left->skew == LEFT) {
			(*n)->skew = (*n)->left->skew = NONE;
			avlrotright(n);
		}
		else {
			switch ((*n)->left->right->skew) {
			case LEFT:
				(*n)->skew = RIGHT;
				(*n)->left->skew = NONE;
				break;

			case RIGHT:
				(*n)->skew = NONE;
				(*n)->left->skew = LEFT;
				break;

			default:
				(*n)->skew = NONE;
				(*n)->left->skew = NONE;
			}
			(*n)->left->right->skew = NONE;
			avlrotleft(& (*n)->left);
			avlrotright(n);
		}
		return OK;

	case RIGHT:
		(*n)->skew = NONE;
		return OK;

	default:
		(*n)->skew = LEFT;
		return BALANCE;
	}
}

enum AVLRES avlrightgrown(struct avlnode **n)
{
	switch ((*n)->skew) {
	case LEFT:
		(*n)->skew = NONE;
		return OK;

	case RIGHT:
		if ((*n)->right->skew == RIGHT) {
			(*n)->skew = (*n)->right->skew = NONE;
			avlrotleft(n);
		}
		else {
			switch ((*n)->right->left->skew) {
			case RIGHT:
				(*n)->skew = LEFT;
				(*n)->right->skew = NONE;
				break;

			case LEFT:
				(*n)->skew = NONE;
				(*n)->right->skew = RIGHT;
				break;

			default:
				(*n)->skew = NONE;
				(*n)->right->skew = NONE;
			}
			(*n)->right->left->skew = NONE;
			avlrotright(& (*n)->right);
			avlrotleft(n);
		}
		return OK;

	default:
		(*n)->skew = RIGHT;
		return BALANCE;
	}
}


enum AVLRES iavl_insert(struct avlnode **n, void *d, int key)
{
	enum AVLRES tmp;

	if(n == NULL) return AVLERROR;

	// base case //
	if (*n == NULL) {
		*n = (struct avlnode *)malloc(sizeof(struct avlnode));

		if (*n == NULL) return AVLERROR;

		(*n)->left = (*n)->right = (struct avlnode*)NULL;
		(*n)->d = d;
		(*n)->key = key;
		(*n)->skew = NONE;
		return BALANCE;
	}

	if (key < (*n)->key)
	{
		tmp = iavl_insert(& (*n)->left, d, key);
		if (tmp == BALANCE)
		{
			return avlleftgrown(n);
		}
		return tmp;
	}
	if (key > (*n)->key) {
		if ((tmp = iavl_insert(& (*n)->right, d, key)) == BALANCE)
		{
			return avlrightgrown(n);
		}
		return tmp;
	}
	return AVLERROR;
}

enum AVLRES avl_insert(AvlTree *n, void *d, int key)
{
	enum AVLRES tmp;

	tmp = iavl_insert(&n->node, d, key);

	if(tmp != AVLERROR)
	{
		n->total++;
	}

	return tmp;
}


enum AVLRES avlleftshrunk(struct avlnode **n)
{
	switch ((*n)->skew) {
	case LEFT:
		(*n)->skew = NONE;
		return BALANCE;

	case RIGHT:
		if ((*n)->right->skew == RIGHT) {
			(*n)->skew = (*n)->right->skew = NONE;
			avlrotleft(n);
			return BALANCE;
		}
		else if ((*n)->right->skew == NONE) {
			(*n)->skew = RIGHT;
			(*n)->right->skew = LEFT;
			avlrotleft(n);
			return OK;
		}
		else {
			switch ((*n)->right->left->skew) {
			case LEFT:
				(*n)->skew = NONE;
				(*n)->right->skew = RIGHT;
				break;

			case RIGHT:
				(*n)->skew = LEFT;
				(*n)->right->skew = NONE;
				break;

			default:
				(*n)->skew = NONE;
				(*n)->right->skew = NONE;
			}
			(*n)->right->left->skew = NONE;
			avlrotright(& (*n)->right);
			avlrotleft(n);
			return BALANCE;
		}

	default:
		(*n)->skew = RIGHT;
		return OK;
	}
}

enum AVLRES avlrightshrunk(struct avlnode **n)
{
	switch ((*n)->skew) {
	case RIGHT:
		(*n)->skew = NONE;
		return BALANCE;

	case LEFT:
		if ((*n)->left->skew == LEFT) {
			(*n)->skew = (*n)->left->skew = NONE;
			avlrotright(n);
			return BALANCE;
		}
		else if ((*n)->left->skew == NONE) {
			(*n)->skew = LEFT;
			(*n)->left->skew = RIGHT;
			avlrotright(n);
			return OK;
		}
		else {
			switch ((*n)->left->right->skew) {
			case LEFT:
				(*n)->skew = RIGHT;
				(*n)->left->skew = NONE;
				break;

			case RIGHT:
				(*n)->skew = NONE;
				(*n)->left->skew = LEFT;
				break;

			default:
				(*n)->skew = NONE;
				(*n)->left->skew = NONE;
			}
			(*n)->left->right->skew = NONE;
			avlrotleft(& (*n)->left);
			avlrotright(n);
			return BALANCE;
		}

	default:
		(*n)->skew = LEFT;
		return OK;
	}
}


int avlfindhighest(struct avlnode *target, struct avlnode **n, enum AVLRES *res)
{
	struct avlnode *tmp;

	*res = BALANCE;

	if (*n == NULL) {
		return 0;
	}

	if ((*n)->right != NULL) {
		if (!avlfindhighest(target, &(*n)->right, res)) {
			return 0;
		}
		if (*res == BALANCE) {
			*res = avlrightshrunk(n);
		}
		return 1;
	}

	target->d = (*n)->d;
	target->key = (*n)->key;
	tmp = *n;
	*n = (*n)->left;
	if(tmp != NULL) free(tmp);
	return 1;
}


int avlfindlowest(struct avlnode *target, struct avlnode **n, enum AVLRES *res)
{
	struct avlnode *tmp;

	*res = BALANCE;
	if (*n == NULL) return 0;

	if ((*n)->left != NULL) {
		if (!avlfindlowest(target, &(*n)->left, res)) {
			return 0;
		}
		if (*res == BALANCE) {
			*res =  avlleftshrunk(n);
		}
		return 1;
	}
	target->d = (*n)->d;
	target->key = (*n)->key;
	tmp = *n;
	*n = (*n)->right;
	if(tmp != NULL) free(tmp);
	return 1;
}


enum AVLRES iavl_remove(struct avlnode **n, int key)
{
	enum AVLRES tmp = BALANCE;

	if(n == NULL) return AVLERROR;

	if (*n == NULL) {
		return AVLERROR;
	}

	if (key < (*n)->key) {
		if ((tmp = iavl_remove(& (*n)->left, key)) == BALANCE) {
			return avlleftshrunk(n);
		}
		return tmp;
	}
	if (key > (*n)->key) {
		if ((tmp = iavl_remove(& (*n)->right, key)) == BALANCE) {
			return avlrightshrunk(n);
		}
		return tmp;
	}

	// if match (key == node->key) //
	if ((*n)->left != NULL) {
		if (avlfindhighest(*n, &((*n)->left), &tmp)) {
			if (tmp == BALANCE) {
				tmp = avlleftshrunk(n);
			}
			return tmp;
		}
	}

	if ((*n)->right != NULL) {
		if (avlfindlowest(*n, &((*n)->right), &tmp)) {
			if (tmp == BALANCE) {
				tmp = avlrightshrunk(n);
			}
			return tmp;
		}
	}

	if(*n != NULL) free(*n);
	*n = (struct avlnode*)NULL;
	return BALANCE;
}

enum AVLRES avl_remove(AvlTree *n, int key)
{
	enum AVLRES tmp = iavl_remove(&n->node, key);

	if(tmp != AVLERROR)
	{
		n->total--;
	}
	return tmp;
}


void *iavl_getvalue(struct avlnode *n, int key)
{
        if (!n) {
                return NULL;
        }
        if (key < (n)->key) {
                return iavl_getvalue(n->left, key);
        }
        if (key > (n)->key) {
                return iavl_getvalue(n->right, key);
        }
        return n->d;
}

void *avl_getvalue(AvlTree n, int key)
{
	return iavl_getvalue(n.node, key);
}

int avl_get_total(AvlTree *n)
{
	return n->total;
}

int iavl_get_indexes(struct avlnode *n, int **indexes, int *i)
{
	if(n == NULL)
	{
		return 0;
	}

	if(n->left != NULL) iavl_get_indexes(n->left, indexes, i);
	(*i)++;
	(*indexes)[*i] = n->key;
	if(n->right != NULL)iavl_get_indexes(n->right, indexes, i);

	return *i;
}

// this function MUST return indexes ordered
int avl_get_indexes(AvlTree *n, int **indexes)
{
	*indexes = (int*)NULL;
	int i = -1;

	if(n->total == 0) return 0;

	*indexes = (int *)malloc(sizeof(int) * n->total);
	iavl_get_indexes(n->node, indexes, &i);
	return n->total;
}

void avl_removeall_internal(struct avlnode **node)
{
	if(node == NULL || *node == NULL) return;

	if((*node)->d != NULL) free((*node)->d);

	(*node)->d = NULL;

	if((*node)->left != NULL) avl_removeall_internal(&(*node)->left);
	if((*node)->right != NULL) avl_removeall_internal(&(*node)->right);

	free(*node);
	*node = (struct avlnode *)NULL;
}

void avl_remove_all(AvlTree *n)
{
	if(n->total == 0) return;

	avl_removeall_internal(&n->node);

	n->node = (struct avlnode *)NULL;
	n->total = 0;
}




