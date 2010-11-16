#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>
#include <lib/scheduler.h>

void main(int argc, char **argv) {
	
  printf("\njedi knight line editor version 0.01");
  
  if (argc == 2) { printf("\ninvoked on file: %s", argv[1]); }

  return 0;
} 
	
