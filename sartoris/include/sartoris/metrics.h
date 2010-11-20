
#ifndef METRICSH
#define METRICSH

#ifdef _METRICS_
struct sartoris_metrics
{
    unsigned int tasks;                 // Tasks created
    unsigned int threads;               // Threads created
	unsigned int smos;
	unsigned int messages;
	unsigned int ports;
    unsigned int indexes;               // Indexes currently used
    /* Memory */
    unsigned int allocated_tasks;       // Tasks allocated with dynamic memory
    unsigned int allocated_threads;     // Threads allocated with dynamic memory
    unsigned int allocated_smos;        // SMOs allocated with dynamic memory
    unsigned int allocated_messages;    // Messages allocated with dynamic memory
    unsigned int allocated_ports;       // Ports allocated with dynamic memory
    unsigned int allocated_indexes;     // Allocated indexes (dynamic memory) *
    unsigned int dynamic_pages;         // How many pages where requested to the OS for dynamic memory
    unsigned int alloc_dynamic_pages;   // How many pages are we holding but not using
};

void initialize_metrics();
int get_metrics(struct sartoris_metrics *m);

extern struct sartoris_metrics metrics;
#endif


#endif

