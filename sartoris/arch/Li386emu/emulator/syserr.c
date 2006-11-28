#include <stdio.h>

/* a la Rochkind */

void syserr(char *msg) {
  extern int errno, sys_nerr;
  
  fprintf(stderr, "ERROR: %s (%d", msg, errno);
  if (errno > 0 && errno < sys_nerr) {
    fprintf(stderr, "; %s)\n", sys_errlist[errno]);
  } else {
    fprintf(stderr, ")\n");
  }
  exit(1);
} 
