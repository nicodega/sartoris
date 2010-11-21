
#include <sartoris/metrics.h>
#include <sartoris/kernel.h>
#include <lib/indexing.h>
#include <sartoris/critical-section.h>

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
        m = MAKE_KRN_PTR(m);
		*m = metrics;
		return SUCCESS;
	}
	return FAILURE;
}

#endif 


