
#include "sartoris/kernel.h"
#include "sartoris/kernel-data.h"
#include "lib/indexing.h"
#include "sartoris/error.h"

int last_error()
{
    return GET_PTR(curr_thread,thr)->last_error;
}
