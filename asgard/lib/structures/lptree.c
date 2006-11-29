/**************************************************************
*   	IMPLEMETATION OF A PATRICIA TRIE (TREE)
*
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

#include <lib/structures/lptree.h>
#include <lib/structures/list.h>
#include <lib/structures/string.h>


int lget_index(char c);
void ipt_init(ipat_tree pt);

int lpt_insert(lpat_tree *lpt, char* key, void* value)
{
	struct _lpat_tree *ipt, *tpt;
	char *c, *pr;
	int klen = len(key);
	int pos = 0, i = 0;
	lnode* n = (lnode*)NULL;
	lpointer *p, *pp, *old_p = (lpointer *)NULL;
	
	// NOTE: this function assumes the key has valid chars //

	// if ipat_tree is empty, diff will be -1 //
	if(lpt->pt == NULL)
	{
		ipt = (struct _lpat_tree*)malloc(sizeof(struct _lpat_tree));
		ipt_init(ipt);

		// tree is empty, insert an lnode //
		ipt->diff = klen;
		ipt->prefix = substring(key,0,klen);
		
		// create the lnode //
		n = (lnode*)malloc(sizeof(lnode));
		n->key = strcopy(key);
		n->value = value;
		n->next = (lnode*)NULL;
		n->prev = (lnode*)NULL;
		lpt->first = n;

		p = (lpointer*)malloc(sizeof(lpointer));
		p->flags = 1;
		p->content = n;
		// insert the lnode on the ipat_tree root //
		ipt->pointers[lget_index(key[klen])] = p;
		ipt->pointer_count = 1;
		lpt->pt = ipt;
	}
	else
	{
		ipt = lpt->pt;

		while(n == NULL)
		{
			if(istrprefix(pos, ipt->prefix,key))
			{
				// prefix is prefix of key //
				/*
					- see if the lpointer at possition
					  is a pt or a content lnode
					- if it's a content lnode we create a new pt there
					- if it's a pt, we continue on the cicle
				*/
				p = ipt->pointers[lget_index(key[pos + ipt->diff])];
				if(p == NULL)
				{
					// create the lnode //
					n = (lnode*)malloc(sizeof(lnode));
					n->key = strcopy(key);
					n->value = value;
					n->next = lpt->first;
					n->prev = (lnode*)NULL;
					if(lpt->first != NULL) lpt->first->prev = n;
					lpt->first = n;

					pp = (lpointer*)malloc(sizeof(lpointer));
					pp->flags = 1;
					pp->content = n;

					ipt->pointers[lget_index(key[pos + ipt->diff])] = pp;
					ipt->pointer_count++;
				}
				else if((p->flags & 0x1) == 0x1)
				{
					// value is a lnode //
					
					/* - if there are many pointers used we have to create a new pt
					   - if there is only one lpointer used, and we have a common prefix
					     larger than the current prefix, we extend the prefix and
						 place the lnodes
					*/

					if(ipt->pointer_count == 1)
					{
#ifdef NOT_OVERWITE
						if(streq(key, ((lnode*)p->content)->key))
						{
							return 0; // key already exists
						}
#endif
						// get the common prefix (at least the diff key)
						pr = strgetprefix(key, ((lnode*)p->content)->key, pos + ipt->diff,0); 

						c = (char *)malloc(len(ipt->prefix) + len(pr) + 1);

						istrcopy(ipt->prefix, c, 0);
						istrcopy(pr,c, ipt->diff);

						free(pr);
						free(ipt->prefix);

						ipt->prefix = c;
						ipt->diff = len(ipt->prefix);
						
						// reallocate old lnode //
						ipt->pointers[lget_index(((lnode*)p->content)->key[pos + ipt->diff])];

						// now place the new lnode // 
						n = (lnode*)malloc(sizeof(lnode));
						n->key = strcopy(key);
						n->value = value;
						n->next = lpt->first;
						n->prev = (lnode*)NULL;
						if(lpt->first != NULL) lpt->first->prev = n;
						lpt->first = n;

						pp = (lpointer*)malloc(sizeof(lpointer));
						pp->flags = 1;
						pp->content = n;

						ipt->pointers[lget_index(key[pos + ipt->diff])] = pp;
						ipt->pointer_count++;
					}
					else
					{
#ifdef NOT_OVERWITE
						if(streq(key, ((lnode*)p->content)->key))
						{
							return 0; // key already exists
						}
#endif

						tpt = (struct _lpat_tree*)malloc(sizeof(struct _lpat_tree));
						ipt_init(tpt);

						tpt->prefix = strgetprefix(((lnode*)p->content)->key,key,pos + ipt->diff,pos + ipt->diff);
						tpt->diff = len(tpt->prefix);

						// insert the new lnode //
						// create the lnode //
						n = (lnode*)malloc(sizeof(lnode));
						n->key = strcopy(key);
						n->value = value;
						n->next = lpt->first;
						n->prev = (lnode*)NULL;
						if(lpt->first != NULL) lpt->first->prev = n;
						lpt->first = n;

						pp = (lpointer*)malloc(sizeof(lpointer));
						pp->flags = 1;
						pp->content = n;

						tpt->pointers[lget_index(key[pos + ipt->diff + tpt->diff])] = pp;

						// insert existing lnode //
						pp = (lpointer*)malloc(sizeof(lpointer));
						pp->flags = 1;
						pp->content = p->content;

						tpt->pointers[lget_index(((lnode*)p->content)->key[pos + ipt->diff + tpt->diff])] = pp;

						// modify current lpointer //
						p->flags = p->flags & 0xFFFFFFFE; // now the lpointer is a pt
						p->content = tpt;

						tpt->pointer_count = 2;


					}

					continue;
				}
				else 
				{
					// p is a pt //
					pos += ipt->diff;
					old_p = p;
					ipt = (struct _lpat_tree*)p->content;
					continue;
				}
			}
			else 
			{
				/*
					algorithm should enter here only if prefix is not a 'prefix' of the key.
					these cases will be taken into account:

						FACT: key has a common prefix with the prefix
							
						Cases:
								- if the current ipt has only one lnode
								  we decrease the current ipt prefix
								- if it has more than one lnode, a new ipt is inserted
						
				*/
			
				if(ipt->pointer_count == 1)
				{
					// see if the lpointer is a lnode //
					p = (lpointer *)NULL;
					i = 0;
					while(p == (lpointer *)NULL)
					{
						p = ipt->pointers[i];
						i++;
					}

					// calculate the prefix //
					if((p->flags & 0x1) == 0x1)
					{
						pr = strgetprefix(key, ((lnode*)p->content)->key, pos,0);	
						tpt = ipt;
						// create lnode lpointer
						pp = p;
							
						tpt->prefix = pr;
						tpt->diff = len(tpt->prefix);

						// delete current p //
						tpt->pointers[i - 1] = (lpointer *)NULL;

						// insert the current pt
						tpt->pointers[lget_index(((lnode*)p->content)->key[pos + tpt->diff])] = pp;
					}
				}
				else
				{
					pr = strgetprefix(key, ipt->prefix, pos,0);
					// trim current ipt prefix //
					c = ipt->prefix;
					ipt->prefix = substring(c,len(pr),len(ipt->prefix) - pos - len(pr));
					ipt->diff = len(ipt->prefix);
					free(c);
					// create new pt
					tpt = (struct _lpat_tree*)malloc(sizeof(struct _lpat_tree));
					ipt_init(tpt);
					// create ipt lpointer
					pp = (lpointer*)malloc(sizeof(lpointer));
					pp->flags = 0;
					pp->content = ipt;

					tpt->prefix = pr;
					tpt->diff = len(tpt->prefix);

					// insert the current pt
					tpt->pointers[lget_index(ipt->prefix[0])] = pp;
				}

				// insert new lnode //
				n = (lnode*)malloc(sizeof(lnode));
				n->key = strcopy(key);
				n->value = value;
				n->next = lpt->first;
				n->prev = (lnode*)NULL;
				if(lpt->first != NULL) lpt->first->prev = n;
				lpt->first = n;

				pp = (lpointer*)malloc(sizeof(lpointer));
				pp->flags = 1;
				pp->content = n;

				tpt->pointers[lget_index(key[pos + tpt->diff])] = pp;

				// if it was not the root
				if(old_p != NULL)
				{
					// update old lpointer //
					old_p->content = tpt;
				}
				else
				{
					lpt->pt = tpt;
				}

				tpt->pointer_count = 2;
			}
		}
	}

	return -1;
}


int lpt_remove(lpat_tree *lpt, char *key)
{
	ipat_tree ipt = lpt->pt;
	int pos = 0, i = 0;
	lpointer *p, *Old_p = (lpointer *)NULL;
	char *c;

	if(ipt == NULL) return 0;

	while(1)
	{
			if(istrprefix(pos, ipt->prefix,key))
			{
				p = ipt->pointers[lget_index(key[pos + ipt->diff])];	
				if(p == NULL)
				{
					// the element does not exist //
					return 0;
				}
				else if((p->flags & 0x00000001) == 0x00000001)
				{
					// it's a lnode //
					
					// fix the list //
					if(((lnode*)p->content)->prev != NULL)
					{
						((lnode*)p->content)->prev->next = ((lnode*)p->content)->next;
					}
					else
					{
						// it was the first node
						lpt->first = ((lnode*)p->content)->next;
					}
					if(((lnode*)p->content)->next != NULL) ((lnode*)p->content)->next->prev = ((lnode*)p->content)->prev;

					// remove it //
					free(((lnode*)p->content)->key);
					free(p->content);
					free(p);
					ipt->pointers[lget_index(key[pos + ipt->diff])] = (lpointer *)NULL;
					ipt->pointer_count--;

					// fix everything //
					if(ipt->pointer_count == 0)
					{
						// ipt is empty //
						if(Old_p == NULL)
						{
							// we are at the root ipt //
							free(ipt->prefix);
							free(ipt);
							lpt->pt = (struct _lpat_tree *)NULL;	// empty the tree
							return -1;		// return true
						}
						else
						{
							// this won't happen I think, because
							// of the next statement...
						}
					}
					else if(ipt->pointer_count = 1)
					{
						// there is only one lnode
						// find it
						p = ipt->pointers[0];
						while(p == NULL)
						{
							i++;
							p = ipt->pointers[i];
						}

						// if ipt is not the root, delete it
						// and place the lnode at the parent pt
						if((p->flags & 0x00000001) == 0x00000001)
						{
							if(Old_p == NULL)
							{
								// ipt is the root //
								// change the prefix //
								c = ipt->prefix;
								ipt->prefix = substring(((lnode*)p->content)->key,0,len(((lnode*)p->content)->key));
								free(c);
								ipt->diff = len(ipt->prefix);
							}
							else
							{
								// delete the ipt and place the lnode on the 
								// last lpointer
								Old_p->flags = p->flags;
								Old_p->content = p->content;
								free(ipt->prefix);
								free(ipt);
								free(p);
							}
						}
						else
						{
							// it's a pt //
							// extend pt's prefix and delete it's parent //
							
							c = (char *)malloc(ipt->diff + ((ipat_tree)p->content)->diff + 1);

							istrcopy(ipt->prefix, c, 0);
							istrcopy(((ipat_tree)p->content)->prefix,c, ipt->diff);

							free(((ipat_tree)p->content)->prefix);
							free(ipt->prefix);
							free(ipt);
							ipt = ((ipat_tree)p->content);
							ipt->prefix = c;
							ipt->diff = len(ipt->prefix);

							if(Old_p != NULL)
							{
								Old_p->content = p->content;
								free(p);
							}
							else
							{
								lpt->pt = (struct _lpat_tree *)p->content;
							}
						}
						return -1;
					}
					else
					{
						return -1; // deleted successfuly
					}
				}
				else
				{
					// it's not an lnode //
					pos += ipt->diff;
					Old_p = p;
					ipt = (ipat_tree)p->content;
					continue;
				}
			}
			else
			{
				// element is not on the trie //
				return 0;
			}
	}

	return -1;
}

void *lpt_getvalue(lpat_tree lpt, char *key)
{
	ipat_tree ipt = lpt.pt;
	int pos = 0, i;
	lpointer *p;
	char *c;

	if(ipt == NULL) return NULL;

	while(1)
	{
		if(istrprefix(pos, ipt->prefix,key))
		{
			p = ipt->pointers[lget_index(key[pos + ipt->diff])];
			if(p == NULL)
			{
				// element does not exist
				return NULL;
			}
			else if((p->flags & 0x00000001) == 0x00000001)
			{
				i = pos + ipt->diff;
				c = ((lnode *)p->content)->key;
				
				while(key[i] != '\0' && c[i] != '\0' && c[i] == key[i])
				{
					i++;
				}

				if(key[i] == c[i])
				{
					return ((lnode *)p->content)->value;
				}
				else
				{
					// keys don't match //
					return NULL;
				}
			}
			else
			{
				// it's a pt //
				pos += ipt->diff;
				ipt = (ipat_tree)p->content;
				continue;
			}
		}
		else
		{
			return NULL;
		}
	}
}

void *lpt_getvalue_parcial_match(lpat_tree lpt, char *key)
{
	return lpt_getvalue_parcial_matchE(lpt, key, (char **)NULL);
}
void *lpt_getvalue_parcial_matchE(lpat_tree lpt, char *key, char **matched)
{
	ipat_tree ipt = lpt.pt;
	int pos = 0, i;
	lpointer *p;
	char *c;

	while(1)
	{
		if(istrprefix(pos, ipt->prefix,key))
		{
			p = ipt->pointers[lget_index(key[pos + ipt->diff])];
			if(p == NULL)
			{
				// a parcial match was fount until ipt, return the \0 element
				p = ipt->pointers[lget_index('\0')];
				if(matched != (char **)NULL)
				{
					*matched = strcopy(((lnode *)p->content)->key);						
				}
				return ((lnode *)p->content)->value;
			}
			else if((p->flags & 0x00000001) == 0x00000001)
			{
				i = pos + ipt->diff;
				c = ((lnode *)p->content)->key;
				
				while(key[i] != '\0' && c[i] != '\0' && c[i] == key[i])
				{
					i++;
				}

				if(c[i] == '\0')
				{
					if(matched != (char **)NULL)
					{
						*matched = strcopy(c);						
					}
					return ((lnode *)p->content)->value;
				}
				else
				{
					// keys don't match //
					return NULL;
				}
			}
			else
			{
				// it's a pt //
				pos += ipt->diff;
				ipt = (ipat_tree)p->content;
				continue;
			}
		}
		else
		{
			return NULL;
		}
	}
}

list lpt_getkeys(lpat_tree lpt)
{
	list lst;
	struct _lnode *n = lpt.first;

	init(&lst);

	while(n != NULL)
	{
		add_head(&lst, n->key);
		n = n->next;
	}
	return lst;
}

list lpt_copy_getkeys(lpat_tree lpt)
{
	list lst;
	char *c;
	struct _lnode *n = lpt.first;

	init(&lst);

	while(n != NULL)
	{
		c = strcopy(n->key);
		add_head(&lst, c);
		n = n->next;
	}

	return lst;
}

list lpt_getvalues(lpat_tree lpt)
{
	list lst;
	struct _lnode *n = lpt.first;
	
	init(&lst);

	while(n != NULL)
	{
		add_head(&lst, n->value);
		n = n->next;
	}
	return lst;
}
int lget_index(char c)
{
	if(c <= 122 && c >= 97)
	{
		return (int)c - 71;
	}

	if(c <= 90 && c >= 65)
	{
		return (int)c - 65;
	}

	if(c <= 57 && c >= 48)
	{
		return (int)c + 14;
	}

	// it's a symbol //
	switch(c)
	{
		break;
		case '-': 
			return 52;
		break;
		case '_': 
			return 53;
		break;
		case '.': 
			return 54;
		break;
		case '[': 
			return 55;
		break;
		case ']':
			return 56;
		break;
		case '?':
			return 57;
		break;
		case '¿':
			return 58;
		break;
		case '!':
			return 59;
		break;
		case '¡': 
			return 60;
		break;
		case '/': 
			return 61;
		break;
		case '\0':
			return 72;
		break;
	}
	return -1;
}

void ipt_init(ipat_tree pt)
{
	int i;

	for(i = 0; i < CHAR_COUNT; i++)
	{
		pt->pointers[i] = (lpointer *)NULL;
	}
	pt->pointer_count = 0;
}

void lpt_init(lpat_tree *lpt)
{
	lpt->first = (lnode *)NULL;
	lpt->pt = (struct _lpat_tree *)NULL;
}

int lpt_is_valid(char c)
{
	return lget_index(c) != -1;
}

#ifdef DRAW


void lpt_drawint(ipat_tree pt, int level)
{
	lpointer *p;
	int i = 0, j;
	static char chars[] = CHARS
	char c[2];
	c[1] = '\0';
	
	
	if(level == 0)
	{
		printf("root--->");
	}
	
	if(pt == NULL) return;

	printf(pt->prefix);
	printf("(%d)",pt->diff);


	while(i < CHAR_COUNT)
	{
		p = pt->pointers[i];
		if(p != NULL)
		{
			if((p->flags & 0x00000001) == 0x00000001)
			{
				printf("\n");
				j = 0;
				while(j <= level)
				{
					printf("\t");
					j++;
				}
				printf("--");
				c[0] = chars[i];
				printf(c);
				printf("->");
				printf(((lnode*)p->content)->key);
			}
			else
			{
				printf("\n");
				j = 0;
				while(j <= level)
				{
					printf("\t");
					j++;
				}
				printf("--");
				c[0] = chars[i];
				printf(c);
				printf("->");
				lpt_drawint((ipat_tree)p->content,level + 1);
			}
		}
		i++;
	}
}


void lpt_draw(lpat_tree lpt)
{
	ipat_tree pt;

	if(lpt.pt == NULL) return;

	pt = lpt.pt;

	// this function creates a visual representation 
	// of the trie
	/*
			root--->PREFIX(diff)
					--->[key]
					--->PREFIX(diff)
						--->[key]
						--->PREFIX(diff)
							--->[key]
						--->PREFIX(diff)
							--->[key]
	*/
	printf("\n\n");
	lpt_drawint(pt,0);
}

#endif
