/*
*
*	OFS (Obsession File system) Multithreaded implementation
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/
	
#include "ofs_internals.h"

int lock_device(int wpid, int deviceid, int logic_deviceid)
{
	int i =0, j = 0, priority = 1;

	if(working_threads[wpid].locked_deviceid != -1) return FALSE;

	wait_mutex(&device_lock_mutex);
	// see if any wp has the device
	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(working_threads[i].locked_deviceid == deviceid && working_threads[i].locked_logic_deviceid == logic_deviceid)
		{
			// get in the waiting array of the thread
			while(j < OFS_MAXWORKINGTHREADS)
			{
				if(priority <= working_threads[i].processes_waiting_lockeddevice[j])
				{
					priority = working_threads[i].processes_waiting_lockeddevice[j] + 1;
				}
				j++;
			}
			priority = working_threads[i].processes_waiting_lockeddevice[wpid] = priority;

			leave_mutex(&device_lock_mutex);

			set_wait_for_signal(wpid, OFS_THREADSIGNAL_DEVICELOCK, -1);
			wait_for_signal(wpid);

			// device was granted
			return TRUE;
		}
		i++;
	}
	
	working_threads[wpid].locked_deviceid = deviceid;
	working_threads[wpid].locked_logic_deviceid = logic_deviceid;

	leave_mutex(&device_lock_mutex);
	return TRUE;
}

int unlock_device(int wpid)
{
	int i =0, j = 0, priority = 0, next_wp = -1;

	if(working_threads[wpid].locked_deviceid == -1) return FALSE;

	wait_mutex(&device_lock_mutex);

	// see we have waiting threads for this device
	while(i < OFS_MAXWORKINGTHREADS)
	{
		if(priority < working_threads[wpid].processes_waiting_lockeddevice[i])
		{
			next_wp = i;
		}
		if(working_threads[wpid].processes_waiting_lockeddevice[i] != 0)
		{
			working_threads[wpid].processes_waiting_lockeddevice[i]--; // decrement priority
		}
		i++;
	}

	if(next_wp != -1)
	{
		// give the device to this process
		// and copy the waiting priorities
		i = 0;
		while(i < OFS_MAXWORKINGTHREADS)
		{
			working_threads[next_wp].processes_waiting_lockeddevice[i] = working_threads[wpid].processes_waiting_lockeddevice[i];
			i++;
		}

		working_threads[next_wp].locked_deviceid = working_threads[wpid].locked_deviceid;
		working_threads[next_wp].locked_logic_deviceid = working_threads[wpid].locked_logic_deviceid;
	}

	// free the device
	working_threads[wpid].locked_deviceid = -1;
	working_threads[wpid].locked_logic_deviceid = -1;

	leave_mutex(&device_lock_mutex);

	// signal the waiting wp selected
	if(next_wp != -1)
	{
		signal(next_wp, NULL, -1, OFS_THREADSIGNAL_DEVICELOCK);
	}

	return TRUE;
}


