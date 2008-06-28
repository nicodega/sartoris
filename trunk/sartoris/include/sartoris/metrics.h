


#ifndef METRICSH
#define METRICSH

#ifdef _METRICS_
struct sartoris_metrics
{
	unsigned int smos;
	unsigned int messages;
	unsigned int ports;
};

void initialize_metrics();
int get_metrics(struct sartoris_metrics *m);

extern struct sartoris_metrics metrics;
#endif


#endif

