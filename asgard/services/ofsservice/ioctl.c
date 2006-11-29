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
			
struct stdfss_res *ioctl(int wpid, struct working_thread *thread, struct stdfss_ioctl *ioctl_cmd)
{
	struct stask_file_info *finf = NULL;
	struct stask_info *tinf = NULL;
	struct stddev_ioctl_cmd ioctl_dev;
	struct stddev_res stddevres;
	struct stdfss_res *ret = NULL;

	// see if this task has the file opened
	if(!get_file_info(thread->taskid, ioctl_cmd->file_id, &finf, &tinf))
	{
		return build_response_msg(thread->command.command, STDFSSERR_FILE_NOTOPEN);
	}

	// check it's a device opened as a file
	if(finf->dinf->mount_info == NULL)
	{
		// it-s not a device, send error msg
		return build_response_msg(thread->command.command, STDFSSERR_NOT_DEVICEFILE);
	}
	
	// issue ioctl to device driver using STDDEV
	ioctl_dev.command = STDDEV_IOCTL;
	ioctl_dev.request = ioctl_cmd->request;
	ioctl_dev.logic_deviceid = finf->logic_deviceid;
	ioctl_dev.param = ioctl_cmd->param;
	ioctl_dev.msg_id = wpid;

	// lock device
	lock_device(wpid, finf->deviceid, finf->logic_deviceid);
	
	working_threads[wpid].locked_device = 1;

	locking_send_msg(finf->deviceid, finf->dinf->stddev_port, &ioctl_dev, wpid);
	
	get_signal_msg((int *)&stddevres, wpid);

	working_threads[wpid].locked_device = 0;

	unlock_device(wpid);

	ret = build_response_msg(thread->command.command, STDFSSERR_OK);

	((struct stdfss_ioctl_res *)ret)->dev_ret = ((struct stddev_ioctrl_res*)&stddevres)->dev_error;

	return ret;
}			
