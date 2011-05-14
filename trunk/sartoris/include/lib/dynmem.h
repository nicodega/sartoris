

#ifndef _DYNMEMH_
#define _DYNMEMH_

void *dyn_alloc_page(int lvl);
void dyn_free_page(void *linear, int lvl);

/* 
f_alloc = allocated dynamic pages (free)
*/

#define DYN_THRESHOLD     4         // max free pages kept (must be divisible by 2)
#define DYN_GRACE_PERIOD  5         // How many free's will we wait 
                                    // until we free some pages. (this is 
                                    // done this way so we are not allocating 
                                    // and deallocating pages all the time)

#endif

