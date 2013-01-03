
#include <sartoris/kernel.h>

#ifndef PRERMISSIONS_H
#define PRERMISSIONS_H

void init_perms(struct permissions *perms);
int validate_perms_ptr(struct permissions *perms, struct permissions *perms_dest, int max, int task);
int test_permission(int task, struct permissions *perms, unsigned int pos);

#endif
