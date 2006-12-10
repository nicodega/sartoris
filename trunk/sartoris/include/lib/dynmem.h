

#ifndef _DYNMEMH_
#define _DYNMEMH_

void *dyn_alloc_page(int lvl);
void dyn_free_page(void *linear, int lvl);

#endif

