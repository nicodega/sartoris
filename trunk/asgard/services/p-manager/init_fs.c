/*
*	Process and Memory Manager Service
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

#include "pmanager_internals.h"
#include <services/atac/atac_ioctrl.h>
#include <services/stds/stddev.h>
#include <services/stds/stdfss.h>

extern int fsinitialized;
extern int fsinitfailed;

int atac_finddev_smo = -1, ofs_path_smo = -1;
struct atac_find_dev_param atacparams;
char *path = "/";

void fsinit_begin()
{
	/* Issue ATAC ATAC_IOCTRL_FINDLDEV */
	struct atac_ioctl_finddev iocmd;

	iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_FINDLDEV;
	iocmd.msg_id = 0;
	iocmd.ret_port = INITFS_PORT;
	iocmd.find_dev_smo = atac_finddev_smo = share_mem(ATAC_TASK, &atacparams, sizeof(struct atac_find_dev_param), READ_PERM | WRITE_PERM);
	
	atacparams.ptype = 0xd0;
	atacparams.bootable = 0;

	send_msg(ATAC_TASK, 4, &iocmd);
}

void fsinit_process_msg()
{
	int taskid;
	struct stddev_ioctrl_res res;
	struct stdfss_init init_msg;	
	struct stdfss_res ofsres;

	while(get_msg_count(INITFS_PORT) > 0)
	{
		if(atac_finddev_smo != -1)
		{
			get_msg(INITFS_PORT, &res, &taskid);

			/* Message expected from atac */
			if(taskid != ATAC_TASK) continue;

			claim_mem(atac_finddev_smo);
			atac_finddev_smo = -1;
			
			if(res.ret != STDDEV_ERR)
			{
				/* Initialize ofs with the logic device found */
				init_msg.command = STDFSS_INIT;
				init_msg.path_smo = ofs_path_smo = share_mem(OFSS_TASK, path, 2, READ_PERM);
				init_msg.ret_port = INITFS_PORT;
				init_msg.deviceid = ATAC_TASK;
				init_msg.logic_deviceid = atacparams.ldevid;

				send_msg(OFSS_TASK, STDFSS_PORT, &init_msg);			
			}
			else
			{
				fsinitfailed = 1;
			}
		}
		else
		{
			get_msg(INITFS_PORT, &ofsres, &taskid);

			if(taskid != OFSS_TASK) continue;
		
			claim_mem(ofs_path_smo);
			ofs_path_smo = -1;

			if(ofsres.ret != STDFSSERR_OK)
			{
				fsinitfailed = 1;
			}
			else
			{
				fsinitialized = 1;
			}
		}
	}
}
