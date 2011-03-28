/*
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


#include "io.h"
#include "types.h"
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include "task_thread.h"
#include "ports.h"
#include <services/atac/atac_ioctrl.h>
#include <services/stds/stddev.h>
#include <services/stds/stdfss.h>
#include <services/stds/stdblockdev.h>
#include "layout.h"

/* If TRUE file system IO has been initialized */
BOOL fsinitialized;
/* If TRUE file system IO has failed */
BOOL fsinitfailed;
/* Holds IO initialization stage */
UINT32 init_io_stage;
/* Main STDFSS service */
UINT16 filesystem_service = 0xFFFF;
/* HDD Service */
UINT16 atac_service = 0xFFFF;

struct atac_find_dev_param atacparams;
INT32 atac_finddev_smo = -1;
INT32 ofs_path_smo = -1;
char *path = "/";

UINT16 io_get_stdfs_process()
{
	return filesystem_service;
}

void swp_io_init(struct swp_io_evt *evt)
{
	evt->callback = NULL;
}

void io_init_source(struct fsio_event_source *iosrc, UINT16 type, UINT16 id)
{
	iosrc->file_id = -1;
	iosrc->fs_service = filesystem_service;
	iosrc->id = id;
	iosrc->type = type;
	iosrc->smo = -1;
	iosrc->flags = 0;
}

void io_init_event(struct io_event *evt, struct fsio_event_source *iosrc)
{
	evt->callback = NULL;
}

void io_findldev();

/* Begin IO subsystem Initialization (find the filesystem root) */
void io_begin_init()
{
	UINT32 i;
	struct atac_ioctl_finddev iocmd;

	fsinitialized = FALSE;
	fsinitfailed = FALSE;
	vmm.swap_present = FALSE;

	init_io_stage = IO_STAGE_FINDDEV;

	/* Initialize IO slots */
	ioslots.free_first = ioslots.slots;

	for(i = 0; i < PMAN_IOSLOTS-1; i++)
	{
		ioslots.slots[i].next_free = &ioslots.slots[i+1];
		ioslots.slots[i].finished.callback = NULL;
		ioslots.slots[i].smo = -1;
		ioslots.slots[i].type = IOSLOT_TYPE_FREE;
	}
	ioslots.slots[i].next_free = NULL;
	ioslots.slots[i].finished.callback = NULL;
	ioslots.slots[i].smo = -1;
	ioslots.slots[i].type = IOSLOT_TYPE_FREE;
	ioslots.free_count = PMAN_IOSLOTS;

    if(atac_service != 0xFFFF)
    {
        if(filesystem_service == 0xFFFF)
        {
            // try to mount a swap drive
            init_io_stage = IO_STAGE_SWAPDEV;

		    io_swap_init();
        }
        else
        {
	        /* Issue ATAC ATAC_IOCTRL_FINDLDEV */
	        io_findldev();
        }
    }
    else
    {
        fsinitfailed = TRUE;
        init_io_stage = IO_STAGE_FINISHED;
    }
}

void io_findldev()
{
    struct atac_ioctl_finddev iocmd;

    iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_FINDLDEV;
	iocmd.msg_id = 0;
	iocmd.ret_port = INITFS_PORT;
    if(init_io_stage == IO_STAGE_WAITATAC)
        iocmd.find_dev_smo = atac_finddev_smo;
    else
	    iocmd.find_dev_smo = atac_finddev_smo = share_mem(atac_service, &atacparams, sizeof(struct atac_find_dev_param), READ_PERM | WRITE_PERM);
		
	/* Find a bootable OFS partition */
	atacparams.ptype = 0xd0;
	atacparams.bootable = 0;

	if(send_msg(atac_service, 4, &iocmd) < 0)
        init_io_stage = IO_STAGE_WAITATAC;
    else
        init_io_stage = IO_STAGE_FINDDEV;
}

/* Internal function, begin Swap partition search */
void io_swap_init()
{
	/* Issue ATAC ATAC_IOCTRL_FINDLDEV */
	struct atac_ioctl_finddev iocmd;

	iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_FINDLDEV;
	iocmd.msg_id = 0;
	iocmd.ret_port = INITFS_PORT;
    if(init_io_stage == IO_STAGE_WAITATACSWP)
        iocmd.find_dev_smo = atac_finddev_smo;
    else
	    iocmd.find_dev_smo = atac_finddev_smo = share_mem(atac_service, &atacparams, sizeof(struct atac_find_dev_param), READ_PERM | WRITE_PERM);
	
	/* Find a bootable OFS partition */
	atacparams.ptype = 0xe0;
	atacparams.bootable = 0;

	if(send_msg(atac_service, 4, &iocmd))
        init_io_stage = IO_STAGE_WAITATACSWP;
    else
        init_io_stage = IO_STAGE_SWAPDEV;
}

void io_mount_root()
{
	struct stdfss_init init_msg;	
    
    init_msg.command = STDFSS_INIT;
	init_msg.ret_port = INITFS_PORT;
	init_msg.deviceid = atac_service;
	init_msg.logic_deviceid = atacparams.ldevid;
    if(init_io_stage == IO_STAGE_WAITOFS)
        init_msg.path_smo = ofs_path_smo;
    else
        init_msg.path_smo = ofs_path_smo = share_mem(filesystem_service, path, 2, READ_PERM);
	
	if(send_msg(filesystem_service, STDFSS_PORT, &init_msg) < 0)
        init_io_stage = IO_STAGE_WAITOFS;
    else					
	    init_io_stage = IO_STAGE_MOUNROOT; // mounting Root
}

/* Check If Initialization finished */
BOOL io_initialized()
{
	int taskid;
	struct stddev_ioctrl_res res;
	struct stdfss_res ofsres;

    // this is in case send message failed
    switch(init_io_stage)
    {
        case IO_STAGE_WAITOFS:
            io_mount_root();
            return FALSE;
        case IO_STAGE_WAITATACSWP:
            io_swap_init();
            return FALSE;
        case IO_STAGE_WAITATAC:
            io_findldev();
            return FALSE;
        default:
            break;
    }

	while(get_msg_count(INITFS_PORT) > 0)
	{		
		if(init_io_stage != IO_STAGE_MOUNROOT)
		{
			get_msg(INITFS_PORT, &res, &taskid);

			/* Message expected from atac */
			if(taskid != atac_service) continue;

            if(atac_finddev_smo != -1) 
				claim_mem(atac_finddev_smo);
			atac_finddev_smo = -1;
			
			if(res.ret != STDDEV_ERR)
			{
				switch(init_io_stage)
				{
					case IO_STAGE_FINDDEV:
						
						/* Initialize ofs with the logic device found */
						io_mount_root();
						break;
					case IO_STAGE_SWAPDEV:
						/* Results for swap check are present */
						vmm_swap_found(atacparams.size - atacparams.metadata_end, atacparams.ldevid);
					
						init_io_stage = IO_STAGE_FINISHED;
						break;
					default:
						break;
				}
			}
			else
			{
				switch(init_io_stage)
				{
					case IO_STAGE_FINDDEV:
						fsinitfailed = TRUE;
						break;
					case IO_STAGE_SWAPDEV:
						/* Results for swap check are present */
						vmm.swap_present = FALSE;
						init_io_stage = IO_STAGE_FINISHED;
						break;
					default:
						break;
				}					
			}
		}
		else
		{
            /* mounting root */
			get_msg(INITFS_PORT, &ofsres, &taskid);

			if(taskid != filesystem_service) continue;

			claim_mem(ofs_path_smo);
			ofs_path_smo = 0xFFFFFFFF;

			if(ofsres.ret != STDFSSERR_OK)
				fsinitfailed = TRUE;
			else
				fsinitialized = TRUE;
			
			/* Ask for swap Initialization */
			init_io_stage = IO_STAGE_SWAPDEV;

			io_swap_init();
		}
	}

	return (init_io_stage == IO_STAGE_FINISHED);
}

/* IO Processing */
void io_process_msg()
{
	INT32 sender_task_id, count;
	struct fsio_event_source *iosrc = NULL;
	struct pm_task *task = NULL;
	struct pm_thread *thread = NULL;
	struct vmm_fmap_descriptor *fm = NULL;
	struct io_slot *ioslot = NULL;
	struct stdfss_res fs_res;
	struct stdblockdev_res res;
	
	/* Process Incoming Task IO responses */
	count = get_msg_count(TASK_IO_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if(get_msg(TASK_IO_PORT, &fs_res, &sender_task_id ) == SUCCESS) 
		{			
			/* Get iosource */
			task = tsk_get(fs_res.thr_id);

			if(task == NULL || task->state == TSK_NOTHING)
			{
				count--;
				continue; // this was not sent by io subsystem
			}

			iosrc = &task->io_event_src; 

			if(!(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE)) 
			{
				count--;
				continue; // this was not sent by io subsystem
			}

			/* If fileopen, set param 0 to file_id */
			if(fs_res.command == STDFSS_OPEN)
			{
				task->io_finished.params[0] = ((struct stdfss_open_res *)&fs_res)->file_id;
			}

			if(iosrc->smo != -1) claim_mem(iosrc->smo);
			iosrc->smo = -1;
			iosrc->flags &= ~IOEVT_FLAG_WAITING_RESPONSE;

			/* Invoke post processing function */
			if(task->io_finished.callback != NULL)
			{
				INT32 (*c)(struct fsio_event_source *, INT32) = task->io_finished.callback;
				task->io_finished.callback = NULL;
				c(iosrc, ((fs_res.ret == STDFSSERR_OK)? IO_RET_OK : IO_RET_ERR));				
			}
		}
		
		count--;
	} // while

	/* Process Incoming Thread IO responses */
	count = get_msg_count(THREAD_IO_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if(get_msg(THREAD_IO_PORT, &fs_res, &sender_task_id ) == SUCCESS) 
		{
			/* Get iodesc */
			thread = thr_get(fs_res.thr_id);

            if(!thread)
            {
				count--;
				continue; // this was not sent by io subsystem
			}

			iosrc = &thread->io_event_src; 

			if(!(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE)) 
			{
				count--;
				continue; // this was not sent by io subsystem
			}

			/* If fileopen, set param 0 to file_id */
			if(fs_res.command == STDFSS_OPEN)
				thread->io_finished.params[0] = ((struct stdfss_open_res *)&fs_res)->file_id;
			
			if(iosrc->smo != -1) claim_mem(iosrc->smo);
			iosrc->smo = -1;
			iosrc->flags &= ~IOEVT_FLAG_WAITING_RESPONSE;
			
			/* Invoke post processing function */
			if(thread->io_finished.callback != NULL)
			{
				INT32 (*c)(struct fsio_event_source *, INT32) = thread->io_finished.callback;
				thread->io_finished.callback = NULL;
				c(iosrc, ((fs_res.ret == STDFSSERR_OK)? IO_RET_OK : IO_RET_ERR));
			}
		}
		
		count--;
	} 

	/* Process incoming FMAP IO responses */
	count = get_msg_count(FMAP_IO_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if (get_msg(FMAP_IO_PORT, &fs_res, &sender_task_id ) == SUCCESS) 
		{
			/* Get iodesc */
			fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(fs_res.thr_id, VMM_MEMREGION_FMAP);
			iosrc = &fm->iosrc; 

			if(!(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE))
			{
				count--;
				continue; // this was not sent by io subsystem
			}

			/* If it was a takeover command, set param 0 to the new file id */
			if(fs_res.command == STDFSS_TAKEOVER)
			{
				fm->io_finished.params[0] = ((struct stdfss_takeover_res*)&fs_res)->file_id;
				fm->io_finished.params[1] = ((struct stdfss_takeover_res*)&fs_res)->perms;
			}

			if(iosrc->smo != -1) claim_mem(iosrc->smo);
			iosrc->smo = -1;
			iosrc->flags &= ~IOEVT_FLAG_WAITING_RESPONSE;
			
			/* Invoke post processing function */
			if(fm->io_finished.callback != NULL)
			{
				INT32 (*c)(struct fsio_event_source *, INT32) = fm->io_finished.callback;
				fm->io_finished.callback = NULL;
				c(iosrc, ((fs_res.ret == STDFSSERR_OK)? IO_RET_OK : IO_RET_ERR));
			}			
		}
		
		count--;
	} 
	/* Process Incoming Swap Read IO responses */
	count = get_msg_count(VMM_READ_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if (get_msg(VMM_READ_PORT, &res, &sender_task_id ) == SUCCESS) 
		{
			thread = thr_get(res.msg_id); 

			/* Invoke post processing function */
			if(thread && thread->swp_io_finished.callback != NULL)
			{
				UINT32 (*sc)(struct pm_thread *, UINT32) = thread->swp_io_finished.callback;
				thread->swp_io_finished.callback = NULL;
				sc(thread, ((res.ret == STDBLOCKDEV_ERR)? IO_RET_ERR : IO_RET_OK));				
			}
		}
		
		count--;
	} 

	/* Process Incoming Swap Write IO responses (IO SLOTS) */
	count = get_msg_count(IOSLOT_WRITE_PORT); // I use a variable to prevent DoS attack on PMAN
	/* Process Incoming File System Write IO responses (IO SLOTS) */
	while(count > 0) 
	{
		if (get_msg(IOSLOT_WRITE_PORT, &res, &sender_task_id ) == SUCCESS) 
		{
			ioslot = &ioslots.slots[res.msg_id];

			/* Invoke ioslot finished function */
			if(ioslot->finished.callback != NULL)
			{
				UINT32 (*iosc)(struct io_slot *, UINT32) = ioslot->finished.callback;
				ioslot->finished.callback = NULL;
				iosc(ioslot, ((res.ret == STDBLOCKDEV_ERR)? IO_RET_ERR : IO_RET_OK));				
			}

			/* Return ioslot to free list */
			ioslot->next_free = ioslots.free_first;
			ioslot->type = IOSLOT_TYPE_FREE;
			ioslot->task_id = 0xFFFF;
			ioslots.free_first = ioslot;
			ioslots.free_count++;
		}
		
		count--;
	}

	count = get_msg_count(IOSLOT_FSWRITE_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if (get_msg(IOSLOT_FSWRITE_PORT, &fs_res, &sender_task_id ) == SUCCESS) 
		{
			ioslot = &ioslots.slots[fs_res.thr_id];

			/* Invoke ioslot finished function */
			if(ioslot->finished.callback != NULL)
			{
				UINT32 (*iosc)(struct io_slot *, UINT32) = ioslot->finished.callback;
				ioslot->finished.callback = NULL;
				iosc(ioslot, ((fs_res.ret == STDFSSERR_OK)? IO_RET_ERR : IO_RET_OK));		
			}

			/* Return ioslot to free list */
			ioslot->next_free = ioslots.free_first;
			ioslot->type = IOSLOT_TYPE_FREE;
			ioslots.free_first = ioslot;
			ioslots.free_count++;
		}
		
		count--;
	}

	/* Process Incoming Swap Write IO responses on task destroy procedure. */
	count = get_msg_count(SWAP_TASK_READ_PORT); // I use a variable to prevent DoS attack on PMAN
	while(count > 0) 
	{
		if (get_msg(SWAP_TASK_READ_PORT, &res, &sender_task_id ) == SUCCESS) 
		{
			task = tsk_get(res.msg_id); 

			/* Invoke post processing function */
			if(task->swp_io_finished.callback != NULL)
			{
				UINT32 (*tsc)(struct pm_task *, UINT32) = task->swp_io_finished.callback;
				task->swp_io_finished.callback = NULL;
				tsc(task, ((res.ret == STDBLOCKDEV_ERR)? IO_RET_ERR : IO_RET_OK));				
			}
		}
		
		count--;
	}
}

/* Begin FS open */
BOOL io_begin_open(struct fsio_event_source *iosrc, char *filename, UINT32 flen, INT32 mode)
{
	struct stdfss_open msg_open;
	struct pm_task *task;
		
	switch(iosrc->type)
	{
		case FILE_IO_TASK:
		case FILE_IO_FMAP:
			task = tsk_get(iosrc->id);

			/* Check if this source has pending IO */
			if(!task || (iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE)) return FALSE;

			iosrc->smo = share_mem(filesystem_service, filename, flen+1, READ_PERM | WRITE_PERM);
            if(iosrc->smo == -1) return FALSE;

			iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;
			
			msg_open.command   = STDFSS_OPEN;
			msg_open.thr_id    = task->id;
			msg_open.file_path = iosrc->smo;
			msg_open.open_mode = mode;
			msg_open.flags     = STDFSS_FILEMODE_MUSTEXIST;
			msg_open.ret_port  = TASK_IO_PORT;

			if(iosrc->type == FILE_IO_FMAP) 
				msg_open.ret_port = FMAP_IO_PORT;

			send_msg(iosrc->fs_service, STDFSS_PORT, &msg_open);

			return TRUE;
			break;
		default:
			break;
	}
	return FALSE;
}

/* Begin an FS read operation */
INT32 io_begin_read(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr)
{
	struct stdfss_read msg_read;

	/* Check if this source has pending IO */
	if(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE) return FALSE;
	
	iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;

	iosrc->smo = share_mem(iosrc->fs_service, ptr, size, READ_PERM | WRITE_PERM);
	iosrc->size = size;

	msg_read.command  = STDFSS_READ;
	msg_read.file_id  = iosrc->file_id;
	msg_read.count    = size;
	msg_read.smo      = iosrc->smo;
	msg_read.thr_id   = iosrc->id;

	switch(iosrc->type)
	{
		case FILE_IO_TASK:
			msg_read.ret_port = TASK_IO_PORT;
			break;
		case FILE_IO_THREAD:
			msg_read.ret_port = THREAD_IO_PORT;
			break;
		case FILE_IO_FMAP:
			msg_read.ret_port = FMAP_IO_PORT;
			break;
	}

	send_msg(iosrc->fs_service, STDFSS_PORT, &msg_read);

	return 0;
}


INT32 io_begin_write(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr)
{
	struct stdfss_write msg_write;

	/* Check if this source has pending IO */
	if(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE) return FALSE;
	
	iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;

	iosrc->smo = share_mem(iosrc->fs_service, ptr, size, READ_PERM);
	iosrc->size = size;

	msg_write.command  = STDFSS_WRITE;
	msg_write.file_id  = iosrc->file_id;
	msg_write.count    = size;
	msg_write.smo      = iosrc->smo;
	msg_write.thr_id   = iosrc->id;

	switch(iosrc->type)
	{
		case FILE_IO_TASK:
			msg_write.ret_port = TASK_IO_PORT;
			break;
		case FILE_IO_THREAD:
			msg_write.ret_port = THREAD_IO_PORT;
			break;
		case FILE_IO_FMAP:
			msg_write.ret_port = FMAP_IO_PORT;
			break;
	}

	send_msg(iosrc->fs_service, STDFSS_PORT, &msg_write);

	return 0;
}


INT32 io_begin_seek(struct fsio_event_source *iosrc, UINT32 offset)
{
	struct stdfss_seek msg_seek;

	msg_seek.command   = STDFSS_SEEK;
	msg_seek.thr_id    = iosrc->id;
	msg_seek.origin    = STDFSS_SEEK_SET;
	msg_seek.file_id   = iosrc->file_id;
	msg_seek.possition = offset;

	iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;

	switch(iosrc->type)
	{
		case FILE_IO_TASK:
			msg_seek.ret_port = TASK_IO_PORT;
			break;
		case FILE_IO_THREAD:
			msg_seek.ret_port = THREAD_IO_PORT;
			break;
		case FILE_IO_FMAP:
			msg_seek.ret_port = FMAP_IO_PORT;
			break;
	}

	send_msg(iosrc->fs_service, STDFSS_PORT, &msg_seek);

	return 0;
}

BOOL io_begin_close(struct fsio_event_source *iosrc)
{
	struct stdfss_close msg_close;

	msg_close.command  = STDFSS_CLOSE;
	msg_close.thr_id   = iosrc->id;
	msg_close.file_id  = iosrc->file_id;
	
	iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;

	switch(iosrc->type)
	{
		case FILE_IO_TASK:
			msg_close.ret_port = TASK_IO_PORT;
			break;
		case FILE_IO_THREAD:
			msg_close.ret_port = THREAD_IO_PORT;
			break;
		case FILE_IO_FMAP:
			msg_close.ret_port = FMAP_IO_PORT;
			break;
	}

	send_msg(iosrc->fs_service, STDFSS_PORT, &msg_close);

	return TRUE;
}


/* Raw Swap Read Operation (Thread bound) */ 
BOOL io_begin_pg_read(UINT32 lba, ADDR ptr, struct pm_thread *thr)
{
	struct stdblockdev_readm_cmd readcmd;
	
	if(!vmm.swap_present) return FALSE;

	/* Share memory to the ATAC */
	thr->vmm_info.fault_smo = share_mem(atac_service, ptr, 0x1000, WRITE_PERM);
		
	/* Send read multiple command to ATAC */
	readcmd.command = BLOCK_STDDEV_READM;
	readcmd.pos = lba;
	readcmd.count = 8;
	readcmd.dev = vmm.swap_ldevid;
	readcmd.buffer_smo = thr->vmm_info.fault_smo;
	readcmd.msg_id = thr->id;
	readcmd.ret_port = VMM_READ_PORT;

	send_msg(atac_service, 4, &readcmd);

	return TRUE;
}

/* Raw Swap Read Operation (Task bound) */ 
BOOL io_begin_task_pg_read(UINT32 lba, ADDR ptr, struct pm_task *task)
{
	struct stdblockdev_readm_cmd readcmd;

	if(!vmm.swap_present) return FALSE;
	
	/* Share memory to the ATAC */
	task->vmm_inf.swap_read_smo = share_mem(atac_service, ptr, 0x1000, WRITE_PERM);
		
	/* Send read multiple command to ATAC */
	readcmd.command = BLOCK_STDDEV_READM;
	readcmd.pos = lba;
	readcmd.count = 8;
	readcmd.dev = vmm.swap_ldevid;
	readcmd.buffer_smo = task->vmm_inf.swap_read_smo;
	readcmd.msg_id = task->id;
	readcmd.ret_port = SWAP_TASK_READ_PORT;

	send_msg(atac_service, 4, &readcmd);

	return TRUE;
}


BOOL io_slot_begin_write(UINT32 possition, ADDR ptr, UINT32 ioslot_id)
{
	struct io_slot *slot = &ioslots.slots[ioslot_id];

	if(!vmm.swap_present) return FALSE;

	if(slot->type == IOSLOT_TYPE_SWAP)
	{
		/* Send an ATAC command */
		struct stdblockdev_writem_cmd writecmd;

		/* Share memory to the ATAC */
		slot->smo = share_mem(atac_service, ptr, 0x1000, READ_PERM);
			
		/* Send read multiple command to ATAC */
		writecmd.command = BLOCK_STDDEV_WRITEM;
		writecmd.pos = possition;
		writecmd.count = 8;
		writecmd.dev = vmm.swap_ldevid;
		writecmd.buffer_smo = slot->smo;
		writecmd.msg_id = ioslot_id;
		writecmd.ret_port = IOSLOT_WRITE_PORT;

		send_msg(atac_service, 4, &writecmd);
	}
	else
	{
		struct stdfss_seek msg_seek;
		struct stdfss_write msg_write;

		/* Seek possition */
		msg_seek.command   = STDFSS_SEEK;
		msg_seek.thr_id    = ioslot_id;
		msg_seek.origin    = STDFSS_SEEK_SET;
		msg_seek.file_id   = slot->dest.fs.file_id;
		msg_seek.possition = possition;
		msg_seek.ret_port  = IOSLOT_FSWRITE_PORT;

		send_msg(slot->dest.fs.task_id, STDFSS_PORT, &msg_seek);

		/* Send an FS read command */
		
		msg_write.command  = STDFSS_READ;
		msg_write.file_id  = slot->dest.fs.file_id;
		msg_write.count    = 0x1000;
		msg_write.smo      = slot->smo;
		msg_write.thr_id   = ioslot_id;
		msg_write.ret_port = IOSLOT_FSWRITE_PORT;

		send_msg(slot->dest.fs.task_id, STDFSS_PORT, &msg_write);
	}

	return TRUE;
}

BOOL io_begin_takeover(struct fsio_event_source *iosrc, UINT32 fileid, UINT16 task)
{
	struct stdfss_takeover msg_tko;
	struct vmm_fmap_descriptor *fm = NULL;

	/* Issue a takeover command to OFS Service */
	switch(iosrc->type)
	{
		case FILE_IO_FMAP:
			fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);
		
			/* Check if this source has pending IO */
			if(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE) return FALSE;
			
			iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;
			iosrc->smo = -1;

			msg_tko.command   = STDFSS_TAKEOVER;
			msg_tko.thr_id    = fm->id;
			msg_tko.ret_port  = FMAP_IO_PORT;
			msg_tko.file_id = fileid;
			msg_tko.task_id = task;

			send_msg(iosrc->fs_service, STDFSS_PORT, &msg_tko);

			return TRUE;
			break;
		case FILE_IO_THREAD:
		case FILE_IO_TASK:
			break;
	}
	return FALSE;
}

BOOL io_begin_release(struct fsio_event_source *iosrc)
{
	struct stdfss_return msg_return;
	struct vmm_fmap_descriptor *fm = NULL;

	/* Issue a return command to OFS Service */
	switch(iosrc->type)
	{
		case FILE_IO_FMAP:
			fm = (struct vmm_fmap_descriptor*)vmm_regiondesc_get(iosrc->id, VMM_MEMREGION_FMAP);

			/* Check if this source has pending IO */
			if(iosrc->flags & IOEVT_FLAG_WAITING_RESPONSE)
				return FALSE;
			
			iosrc->flags |= IOEVT_FLAG_WAITING_RESPONSE;
			iosrc->smo = -1;

			msg_return.command   = STDFSS_RETURN;
			msg_return.thr_id    = fm->id;
			msg_return.ret_port  = FMAP_IO_PORT;
			msg_return.file_id = iosrc->file_id;

			send_msg(iosrc->fs_service, STDFSS_PORT, &msg_return);

			return TRUE;
			break;
		case FILE_IO_THREAD:
		case FILE_IO_TASK:
			break;
	}
	return FALSE;
}

UINT32 ioslot_get_free()
{
	struct io_slot *slot;

	if(ioslots.free_count == 0) return 0xFFFFFFFF;

	slot = ioslots.free_first;
	ioslots.free_first = slot->next_free;
    ioslots.free_count--;
	return (((UINT32)slot) - (UINT32)ioslots.slots) / sizeof(struct io_slot);
}

void ioslot_return(UINT32 id)
{
	struct io_slot *ioslot;

	ioslot = &ioslots.slots[id];
    			
	/* Return ioslot to free list */
	ioslot->next_free = ioslots.free_first;
	ioslot->type = IOSLOT_TYPE_FREE;
	ioslot->task_id = 0xFFFF;
	ioslots.free_first = ioslot;
	ioslots.free_count++;
}
