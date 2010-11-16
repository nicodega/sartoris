#ifndef _PRINTER
#define _PRINTER

#include <sartoris/syscall.h>
#include <services/console/console.h>
#include <oblivion/layout.h>

extern "C" { void reschedule(void); }

class prt {

 public:
  prt();
  int print(char *txt);

 private:
  
};

static bool initzd;
static int csl_num;

#endif
