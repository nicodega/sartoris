
#include "sartoris/metrics.h"
#include "sartoris/kernel.h"

extern struct task tasks[];
extern int curr_task;

#ifdef _METRICS_

struct sartoris_metrics metrics;

void initialize_metrics()
{
	metrics.smos = 0;
	metrics.messages = 0;
	metrics.ports = 0;
}

int get_metrics(struct sartoris_metrics *m)
{
	if(VALIDATE_PTR(m)) 
	{
		*m = metrics;
		return SUCCESS;
	}
	return FAILURE;
}

#endif 


