#include <stdio.h>

#include "../page_list.h"



int main(char **argv, int argc) {

  
  page_list pl;
  char *pool = malloc(0x1000000);
  int i;
  int ok;
  
  printf("\ninitailizing page list...");

  init_page_list(&pl);

  printf("done\n\n");
  
  printf("one page (contiguous elements) test cases\n");
  
  printf("page list has %u items.(expected 0)\n", page_count(&pl));

  put_page(&pl, pool+PAGE_SIZE*2);
  put_page(&pl, pool+PAGE_SIZE);
  put_page(&pl, pool);
 
  printf("page list has %u items (expected 3) \n", page_count(&pl));
  
  printf("get page returned %x (expected %x)\n", get_page(&pl), pool);
  
  printf("page list has %u items (expected 2)\n", page_count(&pl));

  printf("get page returned %x (expected %x)\n", get_page(&pl), pool+PAGE_SIZE);

  printf("page list has %u items (expected 1)\n", page_count(&pl));

  printf("get page returned %x (expected %x)\n", get_page(&pl), pool+PAGE_SIZE*2);
  
  printf("\npage list has %u items (expected 0)\n", page_count(&pl));  
  printf("top page count is %u (expected 0)\n", pl.top_page_entries);
  printf("top page points to %x (expected 0)\n", pl.top_page);

  printf("\nmany pages (bulk insert) test cases\n");
  printf("starting insertion test...");
    
  for(i=0; i<4096; i++) {

    put_page(&pl, pool+PAGE_SIZE*i);
    
  }
  printf("done\n");

  printf("page list has %u items (expected %u)\n", page_count(&pl), 4096);
  
  printf("starting extraction test...");
  

  ok = 1;
  
  for(i=4096-1; ok && i>=0; i--) {
   
    ok = (get_page(&pl) == pool+PAGE_SIZE*i);
    
  }
  
  if (ok) {
    printf("done, results seem OK\n");
    printf("page list has %u items (expected 0)\n", page_count(&pl));
  } else {
    printf("done, results differ form expected ones\n");
    printf("page list has %u items (expected who knows)\n", page_count(&pl));
  }

  return 0;

}
