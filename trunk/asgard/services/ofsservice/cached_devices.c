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

/* cached devices management */
device_info *get_cache_device_info(int deviceid, int logic_deviceid)
{
	device_info *dinf = NULL;

	wait_mutex(&cached_devices_mutex);

	AvlTree *sub_avl = (AvlTree *)avl_getvalue(cached_devices, deviceid);

	if(sub_avl == NULL)
	{
		leave_mutex(&cached_devices_mutex);
		return NULL;
	}

	dinf = (device_info *)avl_getvalue(*sub_avl, logic_deviceid);

	leave_mutex(&cached_devices_mutex);

	return dinf;
}
device_info *cache_device_info(int deviceid, int logic_deviceid, struct sdevice_info *dev_inf)
{
	AvlTree *sub_avl = NULL;
	device_info *dinf = NULL;

	if(cached_devices_count >= OFS_MAX_CACHED_DEVICES)
	{
		remove_cached_device();
	}

	wait_mutex(&cached_devices_mutex);
	sub_avl = (AvlTree *)avl_getvalue(cached_devices, deviceid);

	if(sub_avl == NULL)
	{
		// the device has never being used //
		// we have to create the node //
		// and the logic device in the sub avl tree //
		sub_avl = (AvlTree *)malloc(sizeof(AvlTree));

		avl_init(sub_avl);

		// insert the logic_device in the sub_tree //
		if(dev_inf == NULL)
		{
			dinf = (device_info *)malloc(sizeof(device_info));
		}
		else
		{
			dinf = dev_inf;
		}

		cached_devices_count++;

		avl_insert(sub_avl, (void *)dinf, logic_deviceid);

		// insert the logic devices sub tree on the devices tree //
		avl_insert(&cached_devices, (void *)sub_avl, deviceid);

	}
	else
	{
		// see if the logic device is present //
		dinf = (device_info *)avl_getvalue(*sub_avl, logic_deviceid);

		if(dinf == NULL)
		{
			// insert the logic_device in the sub_tree //
			if(dev_inf == NULL)
			{
				dinf = (device_info *)malloc(sizeof(device_info));
			}
			else
			{
				dinf = dev_inf;
			}

			cached_devices_count++;
		
			// insert dev inf for logic device //
			avl_insert(sub_avl, (void *)dinf,logic_deviceid);
		}
	}

	leave_mutex(&cached_devices_mutex);

	return dinf;
}	
void remove_cached_device()
{
	int *service_indexes = NULL, *logic_indexes = NULL;
	int candidate = -1;
	int candidate_logic = -1;
	int candidate_logic_hits = -1;
	int services_total = -1;
	int logic_total = -1;
	int i = 0, j = 0;
	device_info *logic_device = NULL;
	AvlTree *sub_avl = NULL;

	wait_mutex(&cached_devices_mutex);
	avl_get_indexes(&cached_devices, &service_indexes);

	if(service_indexes == NULL)
	{
		leave_mutex(&cached_devices_mutex);
		return;
	}

	services_total = avl_get_total(&cached_devices);

	while(i < services_total)
	{
		sub_avl = (AvlTree *)avl_getvalue(cached_devices, service_indexes[i]);
		if(sub_avl != NULL)
		{
			candidate = service_indexes[i];
			
			avl_get_indexes(sub_avl, &logic_indexes);

			j = 0;

			logic_total = avl_get_total(sub_avl);

			while(j < logic_total)
			{
				logic_device = (device_info*)avl_getvalue(*sub_avl, logic_indexes[j]);

				// NOTE: This might apear to produce a deadlock
				// because we are inside other mutex.
				// I've given it a deep thought and believe
				// it's safe. I could be wrong but hey, I gave it a thougth!
				wait_mutex(&logic_device->processing_mutex);

				// NOTE: I won't enter logic_device->blocked_mutex, because if it's blocked it's not necesary
				// and if it's not, then if a char operation is being executed, procesing count will not be 0
				// hence, the device is not a candidate for removal anyway

				if(logic_device->hits > candidate_logic_hits && avl_get_total(&logic_device->procesing) == 0 && length(&logic_device->waiting) == 0 && !logic_device->blocked)
				{
					candidate_logic = logic_indexes[j];
					candidate_logic_hits = logic_device->hits;
				}

				leave_mutex(&logic_device->processing_mutex);

				j++;
			}

			free(logic_indexes);
		}
		i++;
	}

	logic_device = NULL;

	if(candidate != -1 && candidate_logic != -1)
	{
		sub_avl = (AvlTree *)avl_getvalue(cached_devices, candidate);
		
		logic_device = (device_info *)avl_getvalue(*sub_avl, candidate);

		avl_remove(sub_avl, candidate_logic);

		if(avl_get_total(sub_avl) == 0)
		{
			// remove device tree
			avl_remove(&cached_devices, candidate);
		}
	}

	if(service_indexes != NULL) free(service_indexes);
	if(logic_indexes != NULL) free(logic_indexes);

	leave_mutex(&cached_devices_mutex);

	// using the mutex here is necesary for the device might not be locked
	wait_mutex(&opened_files_mutex);
	if(logic_device != NULL && logic_device->mount_info == NULL && logic_device->open_count == 0)
	{
		// free the device
		free_device_struct(logic_device);
	}
	leave_mutex(&opened_files_mutex);

	return;
}


void remove_cached_deviceE(int deviceid, int logic_deviceid)
{
	device_info *logic_device = NULL;
	AvlTree *sub_avl = NULL;

	wait_mutex(&cached_devices_mutex);
	
	sub_avl = (AvlTree *)avl_getvalue(cached_devices, deviceid);
	
	if(sub_avl != NULL)
	{
		logic_device = (device_info*)avl_getvalue(*sub_avl, logic_deviceid);

		avl_remove(sub_avl, logic_deviceid);

		if(avl_get_total(sub_avl) == 0)
		{
			// remove device tree
			avl_remove(&cached_devices, deviceid);
		}	
	}

	leave_mutex(&cached_devices_mutex);
}
