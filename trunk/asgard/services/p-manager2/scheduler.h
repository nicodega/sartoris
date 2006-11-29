/*
	Process Manager Scheduler Include
*/

#ifndef PMAN_SCHEDULERH
#define PMAN_SCHEDULERH

#include "types.h"
#include <services/pmanager/signals.h>

#define SCHED_MAXPRIORITY 5
#define SCHED_DEFAULTPRIORITY 0

struct sch_node
{
	struct pm_thread *next;
	struct pm_thread *prev;
	UINT32 priority;		/* thread priority                              */
	UINT16 quantums;		/* Quantums assigned to this thread so far		*/
	UINT16 recursion;
	BOOL blocked;
} PACKED_ATT;

/* Scheduler List */
struct sch_list
{
	struct pm_thread *running;
	struct pm_thread *last_runned;
	struct pm_thread *first[SCHED_MAXPRIORITY];
	struct pm_thread *last[SCHED_MAXPRIORITY];
	INT32             total_threads;
	INT32             active_threads;

	int list_selector;	// list from where we must take the next running thread
	struct pm_thread *first_blocked;
} scheduler;

void sch_init();
int sch_schedule();

void sch_deactivate(struct pm_thread *thr);
void sch_activate(struct pm_thread *thr);

// add a thread to the scheduler as inactive
void sch_add(struct pm_thread *thr);
// remove a thread from scheduler
void sch_remove(struct pm_thread *thr);

void sch_init_node(struct sch_node *n);
unsigned short sch_priority_quantum(INT32 priority);

/* Reschedule a thread */
void sch_reschedule(struct pm_thread *thr, UINT32 possition);

/* Force executing thread completion and send it to the end of the list. */
void sch_force_complete();

/* Get running thread */
UINT16 sch_running();

#endif

