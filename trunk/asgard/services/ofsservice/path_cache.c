/*
*	This is a very (VERY) simple path cache for the of service.
*	But still more efficient than parsing directories many times.
*
*	OFS (Obsession File system) Multithreaded implementation
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

#include "ofs_internals.h"

void preinit_path_cache(struct path_cache *pc)
{
	pc->root = NULL;
	pc->dinf = NULL;
}

void init_path_cache(struct path_cache *pc, struct sdevice_info *dinf, char *mount_path)
{
	pc->root = (struct cache_node*)malloc(sizeof(struct cache_node));
	pc->root->path = mount_path;
	pc->root->node_id = 0;
	pc->root->node = nref(dinf, 0);
	pc->root->hits = 0;
	pc->root->size = sizeof(struct cache_node) + len(mount_path);
	init(&pc->root->childs);

	pc->dinf = dinf;

	init_mutex(&pc->mutex);
}

/* Will return -1 if path is not cached */
struct gc_node *get_cached_node(struct path_cache *pc, char *path, unsigned int *flags, struct gc_node **parent, unsigned int *dir_parentid)
{
	int start = 0;

	if(streq(pc->root->path, path))
	{
		*flags = -1;
		if(parent != NULL) *parent = NULL;
		if(dir_parentid != NULL) *dir_parentid = -1;
		return pc->root->node;
	}

	wait_mutex(&pc->mutex);

	struct cache_node *cn = get_closest_match(pc, path, &start);

	if(cn != NULL && start == last_index_of(path, '/') + 1)
	{
		cn->hits++;
		*flags = cn->flags;
		if(parent != NULL) *parent = nref(pc->dinf, cn->parent->node->nodeid);
		if(dir_parentid != NULL) *dir_parentid = ((cn->parent->parent == NULL)? -1 : cn->parent->parent->node->nodeid);

		leave_mutex(&pc->mutex);

		return nref(pc->dinf, cn->node->nodeid);
	}
	leave_mutex(&pc->mutex);

	return NULL; // miss
}

/* NOTE: node id */
void cache_path_node(struct path_cache *pc, char *path, unsigned int nodeid, unsigned int flags)
{
	int start = 0, end = 0;

	wait_mutex(&pc->mutex);

	struct cache_node *cn = get_closest_match(pc, path, &start);

	if(start == last_index_of(path, '/') + 1)
	{
		cn->hits++;
		cn->flags = flags;
		leave_mutex(&pc->mutex);
		return;
	}

	// add the node to the cache
	struct cache_node *newcn = (struct cache_node *)malloc(sizeof(struct cache_node));

	start = start + len(cn->path) + ((cn == pc->root)? 0 : 1);
	end = len(path);

	char *npath = (char *)malloc(end - start + 1);
	estrcopy(path, npath, start, end - start + 1);

	newcn->path = npath;
	newcn->node_id = nodeid;
	newcn->node = nref(pc->dinf, nodeid);
	newcn->hits = 2;
	newcn->flags = flags;
	newcn->size = sizeof(struct cache_node) + end - start + 1;
	newcn->parent = cn;
	init(&newcn->childs);

	// see if any childs can be promoted to the new node
	CPOSITION it = get_head_position(&cn->childs);
	CPOSITION cit = NULL;
	while(it != NULL)
	{
		cit = it;
		struct cache_node *childcn = (struct cache_node *)get_next(&it);
		if(strprefix(npath, childcn->path))
		{

			// promote child
			int i = len(newcn->path), j = 0;
			while(childcn->path[i] != '\0')
			{
				childcn->path[j] = childcn->path[i];
				j++;
				i++;
			}
			childcn->path[j] = '\0';
			remove_at(&cn->childs, cit);
			add_tail(&newcn->childs, childcn);	
			newcn->size += childcn->size;
			childcn->parent = newcn;
		}
	}

	// cleanup cache
	cleanup(pc, cn);
	
	// add the new node to the cache
	add_tail(&cn->childs, newcn);	

	leave_mutex(&pc->mutex);
}

struct cache_node *get_closest_match(struct path_cache *pc, char *path, int *out_pos)
{
	int start = len(pc->root->path);
	int end = len(path), cnlen, lastbreak = last_index_of(path, '/');
	struct cache_node *cn = NULL;
	struct cache_node *ccn = pc->root;

	*out_pos = 0;

	if(streq(pc->root->path, path)) return pc->root;

	wait_mutex(&pc->mutex);

	CPOSITION it = get_head_position(&pc->root->childs);

	while(it != NULL)
	{
		cn = (struct cache_node *)get_next(&it);

		cnlen = len(cn->path);

		if(istrprefix(start, cn->path, path) && ((path[start + cnlen] == '\0') || (path[start + cnlen] == '/'))) // FIXED check for equal or containing directory
		{
			// current node is a prefix of the path
			// hence set iterator to this node first 
			// child and continue
			it = get_head_position(&cn->childs);
			*out_pos = start;
			start += cnlen + 1; // 1 is added for missing /
			cn->hits++;
			ccn = cn;

			if(start > lastbreak) break; // FIXED: Added check for search end.
		}
	}		

	leave_mutex(&pc->mutex);
	return ccn;
}

struct cache_node *get_candidate(struct path_cache *pc, struct cache_node *cn, CPOSITION *candidate_it, struct cache_node **candidate_parent, unsigned int *chits, struct cache_node *ignore_cn)
{
	CPOSITION it = get_head_position(&cn->childs);
	CPOSITION cit = NULL;
	struct cache_node *can = NULL;

	while(it != NULL)
	{
		struct cache_node *cnode = (struct cache_node *)get_next(&it);
		if(cnode != ignore_cn)
		{
			*candidate_parent = cn;
			can = get_candidate(pc, cnode, candidate_it, candidate_parent, chits, ignore_cn);
			if(can != NULL)
			{
				*candidate_it = cit;
				*candidate_parent = cn;
			}
		}
	}

	if(*chits > cn->hits && cn != pc->root)
	{
		// this node is a better candidate
		*chits = cn->hits;
		return cn;
	}
	else if(can != NULL)
	{
		return can;
	}
	return NULL;
}

void clear_node(struct path_cache *pc, struct cache_node *cn);

void cleanup(struct path_cache *pc, struct cache_node *cn)
{
	/*
		This function will attempt reducing the size of the cache 
		until no more nodes can be freed, or size is lower than
		OFS_MAX_PATHCACHE_SIZE 
	*/
	if(pc->root->size <= OFS_MAX_PATHCACHE_SIZE) return;

	// we will ignore the cn branch, for it's a node
	// being added to the cache
	
	CPOSITION candidate_it = NULL;
	struct cache_node *candidate_parent = NULL;
	unsigned int chits = (unsigned int)-1;

	struct cache_node *candidate = NULL;

	while(pc->root->size > OFS_MAX_PATHCACHE_SIZE && candidate != NULL)
	{
		candidate = get_candidate(pc, pc->root, &candidate_it, &candidate_parent, &chits, cn);
		if(candidate != NULL)
		{
			remove_at(&candidate_parent->childs, candidate_it);
			candidate_parent->size -= candidate->size;
			clear_node(pc, candidate);
		}
	}
}

void clear_node(struct path_cache *pc, struct cache_node *cn)
{
	CPOSITION it = get_head_position(&cn->childs);

	while(it != NULL)
	{
		struct cache_node *ccn = (struct cache_node *)get_next(&it);
		clear_node(pc, ccn);
	}
	lstclear(&cn->childs);
	nfree(pc->dinf, cn->node);
	free(cn->path);
	free(cn);
}

void free_cache(struct path_cache *pc)
{
	if(pc->root == NULL) return;
	clear_node(pc, pc->root);
	pc->root == NULL;
	pc->dinf = NULL;
}

void invalidate_cache_path(struct path_cache *pc, char *path)
{
	if(streq(pc->root->path, path))
	{
		return;
	}

	wait_mutex(&pc->mutex);
	int start;

	struct cache_node *cn = get_closest_match(pc, path, &start);

	if(cn != NULL && start == last_index_of(path, '/') + 1)
	{
		CPOSITION it = get_head_position(&cn->parent->childs);
		CPOSITION cit = NULL;
		while(it != NULL)
		{
			cit = it;
			struct cache_node *ccn = (struct cache_node *)get_next(&it);
			if(streq(ccn->path, &path[last_index_of(path, '/') + 1]))
			{
				remove_at(&cn->parent->childs, cit);
				break;
			}
		}
		clear_node(pc, cn);
	}
	leave_mutex(&pc->mutex);
}
