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

#include <lib/dirlib.h>
#include <lib/printf.h>
#include <lib/debug.h>
#include <lib/iolib.h>
#include <lib/structures/list.h>

#include <services/stds/stddev.h>
#include <services/stds/stdservice.h>
#include <services/stds/stdblockdev.h>
#include <services/atac/atac_ioctrl.h>

int stddevport = -1;
int protocol_port = -1;
int aid;
int nids;

char *str_help = "\n\nAvailable commands are:\n\t- h Show this help.\n\t- l List current partitions.\n\t- c Create partition.\n\t- a Create alt os partition.\n\t- r Remove partition.\n\t- Q Exit fdisk without saving\n\t- S Preserve changes and exit.";

list commands;
list devices;

#define FDISK_REMOVE	1
#define FDISK_CREATE	2

struct fdisk_command
{
	int command;	
	int deviceid;
	int intid;
	unsigned int slba;
	unsigned int size;
	unsigned char type;
};

struct fdisk_dev
{
	int intid;
	int new;
	int removed;
	unsigned int id;
	unsigned int slba;
	unsigned int size;
	unsigned int type;
	unsigned short phid;
};

void build_partition_list();
int list_partitions(int phyonly, int usephyid, unsigned short phyid);
void create_partition();
void remove_partition();
int apply_changes();
void sort_list();
int apply_create(struct fdisk_command *cmd);
int apply_remove(struct fdisk_command *cmd);

int main(int argc, char **argv) 
{
	// let's test the ata controler service logic device management :D
	int storage_size = 0, id;

	aid = dir_resolveid("devices/atac");
	
	struct stddev_devtype_res devtype_res;
	struct stdservice_query_interface query_cmd;
	struct stdservice_query_res query_res;
	struct stddev_res stddevres;
	struct stddev_cmd stddevcmd;

	init(&commands);
	init(&devices);

	// use stdservice query interface to get stddev port
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STDDEV_UIID;
	query_cmd.ret_port = 7;
	query_cmd.ver = STDDEV_VERSION;
	query_cmd.msg_id = 0;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);

	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &query_res, &id);

	if(query_res.ret == STDSERVICE_RESPONSE_FAILED)
	{
		return 1;
	}

	stddevport = query_res.port;

	// get device type //
	stddevcmd.command = STDDEV_GET_DEVICETYPE;
	stddevcmd.ret_port = 7;
	stddevcmd.msg_id = 1;
	
	send_msg(aid, stddevport, &stddevcmd);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &devtype_res, &id);

	if(devtype_res.dev_type != STDDEV_BLOCK)
	{
		return 1;
	}

	// issue query interface command for stdblockdev //
	query_cmd.command = STDSERVICE_QUERYINTERFACE;
	query_cmd.uiid = STD_BLOCK_DEV_UIID;
	query_cmd.ret_port = 7;
	query_cmd.ver = STD_BLOCK_DEV_VER;
	query_cmd.msg_id = 1;

	send_msg(aid, STDSERVICE_PORT, &query_cmd);
	
	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &query_res, &id);

	if(query_res.ret != STDSERVICE_RESPONSE_OK)
	{
		return 1;
	}

	protocol_port = query_res.port;

	/* All set to comunicate with atac service. 
	Fetch devices with IOCTRL enum devices command. */
	build_partition_list();

	printf("\nfdisk 0.00001 (pediatrical version)\n\nEnter h for help.\n\nCommand: ");
	
	char cmd;
	int modified = 0;

	cmd = fgetc(&stdin);

	while(cmd != 'Q')
	{
		switch(cmd)
		{
			case 'h':
				printf(str_help);
				break;
			case 'l':
				printf("\n\nDevices list:\n");
				list_partitions(0,0,0);
				break;
			case 'c':
				create_partition();
				break;
			case 'a':
				break;
			case 'r':
				remove_partition();
				break;
			case 'S':
				apply_changes();
				break;
		}

		printf("\nCommand: ");
		
		cmd = fgetc(&stdin);

		if(cmd == 'Q' && length(&commands) != 0)
		{
			printf("There are pending changes, do you want to discard them? [N]: ");
			cmd = fgetc(&stdin);
			if(cmd == 'y' || cmd == 'Y')
			{
				cmd = 'Q';
			}
			else
			{
				printf("\nCommand: ");
				cmd = fgetc(&stdin);
			}
		}
	}

	return 0;
} 
	
void build_partition_list()
{
	/* Use IOCTRL command ATAC_IOCTRL_ENUMDEVS :D */	
	struct atac_enum_dev_param params;
	struct atac_ioctl_enumdevs cmd;
	struct stddev_ioctrl_res res;
	int taskid;
	struct fdisk_dev *dev = NULL;

	params.continuation = 0;

	cmd.command = STDDEV_IOCTL;
	cmd.request = ATAC_IOCTRL_ENUMDEVS;
	cmd.enum_dev_smo = share_mem(aid, &params, sizeof(struct atac_enum_dev_param), READ_PERM | WRITE_PERM);
	cmd.msg_id = 0;
	cmd.ret_port = 7;

	remove_all(&devices);

	nids = 0;

	do
	{
		send_msg(aid, stddevport, &cmd);

		while(get_msg_count(7) == 0){ reschedule(); }

		get_msg(7, &res, &taskid);

		if(res.ret != STDDEV_ERR)
		{
			dev = (struct fdisk_dev*)malloc(sizeof(struct fdisk_dev));

			dev->id = params.devinf.id;
			dev->slba = params.devinf.start_lba;
			dev->size = params.devinf.size;
			dev->type = params.devinf.ptype;
			dev->new = 0;
			dev->intid = nids++;
			dev->phid = params.devinf.pid;
			dev->removed = 0;

			add_tail(&devices, dev);	
		}

	}while(res.ret != STDDEV_ERR);

	claim_mem(cmd.enum_dev_smo);
}

int list_partitions(int phyonly, int usephyid, unsigned short phyid)
{
	struct fdisk_dev *dev = NULL;
	int ret = 0;

	CPOSITION it = get_head_position(&devices);

	while(it != NULL)
	{
		dev = (struct fdisk_dev*)get_next(&it);

		if((usephyid && dev->phid == phyid) || !usephyid)
		{
			if(dev->id & STDDEV_PHDEV_FLAG)
			{
				printf("\n-> P id=%i, ldevid=%i, size=%i sec", dev->intid, dev->id, dev->size);
				ret++;
			}
			else if(!phyonly && !dev->removed)
			{
				printf("\n  +- %s id=%i, phyid=%i, ldevid=%i, slba=%i, size=%i sec, type=%x", (dev->new)? "N" : "L", dev->intid, dev->phid, dev->id, dev->slba, dev->size, dev->type);
				ret++;
			}
		}
	}

	printf("\n");

	return ret;
}

void create_partition()
{
	char buff[50];

	/* Create a partition command, and modify devices list */
	
	/* This is not ment to be a complete fdisk program, I won't 
	be performing checks. */
	struct fdisk_command *cmd = (struct fdisk_command*)malloc(sizeof(struct fdisk_command));

	cmd->command = FDISK_CREATE;	
	
	printf("\n\nPhysical disks:\n");

	list_partitions(1,0,0);

	printf("\nPhysical disk id for partition: ");

	char *str = fgets(buff, 49, &stdin);

	if(str != NULL && isnumeric(str))
	{
		cmd->deviceid = atoi(str);
	}
	else
	{
		printf("\nInvalid id");
		free(cmd);
		return;
	}

	/* get physical device id using internal id */
	CPOSITION it = get_head_position(&devices);
	struct fdisk_dev *dev = NULL;

	unsigned short phyid = (unsigned short)-1;
	while(it != NULL)
	{
		dev = (struct fdisk_dev*)get_next(&it);
		if(dev->intid == cmd->deviceid)
		{
			cmd->deviceid = phyid = dev->id;
			break;
		}
	}

	if(phyid == (unsigned short)-1)
	{
		printf("\nInvalid id");
		free(cmd);
		return;
	}

	printf("\n\nPhysical disk partitions info:\n");

	list_partitions(0, 1, phyid);

	printf("\nStart lba: ", phyid);

	str = fgets(buff, 50, &stdin);

	if(str != NULL && isnumeric(str))
	{
		cmd->slba = atoi(str);
	}
	else
	{
		printf("\nInvalid lba");
		free(cmd);
		return;
	}

	printf("\nSize (sectors): ");

	str = fgets(buff, 50, &stdin);

	if(str != NULL && isnumeric(str))
	{
		cmd->size = atoi(str);
	}
	else
	{
		printf("\nInvalid size");
		free(cmd);
		return;
	}

	printf("\nType (integer): ");

	str = fgets(buff, 50, &stdin);

	if(str != NULL && isnumeric(str))
	{
		cmd->type = atoi(str);
	}
	else
	{
		printf("\nInvalid type");
		free(cmd);
		return;
	}

	add_tail(&commands, cmd);

	/* create a device */
	dev = (struct fdisk_dev*)malloc(sizeof(struct fdisk_dev));

	dev->intid = nids++;
	dev->id = 0;
	dev->slba = cmd->slba;
	dev->size = cmd->size;
	dev->type = cmd->type;
	dev->phid = phyid;
	dev->new = 1;
	dev->removed = 0;

	cmd->intid = dev->id;

	/* Find insert possition */
	it = get_head_position(&devices);
	CPOSITION cit = NULL;

	while(it != NULL)
	{
		cit = it;
		struct fdisk_dev *cdev = (struct fdisk_dev*)get_next(&it);

		if((cdev->id & STDDEV_PHDEV_FLAG) && cdev->id == cmd->deviceid)
		{
			break;
		}
	}

	insert_after(&devices, cit, dev);

	sort_list();
}

void remove_partition()
{
	char buff[50];

	/* Schedule partition removal, if it was new delete it and it's command */
	printf("\n\nLogic devices:\n");

	list_partitions(0,0,0);

	printf("\nSelect partition being removed: ");

	char *str = fgets(buff, 50, &stdin);
	int intid = -1;

	if(str != NULL && isnumeric(str))
	{
		intid = atoi(str);
	}
	else
	{
		printf("\nInvalid id");
		return;
	}

	/* Get device with intid specificated */
	CPOSITION it = get_head_position(&devices);
	CPOSITION cit = NULL;
	struct fdisk_dev *dev = NULL;

	while(it != NULL)
	{
		cit = it;
		struct fdisk_dev *cdev = (struct fdisk_dev*)get_next(&it);

		if(cdev->intid == intid)
		{
			dev = cdev;
			break;
		}
	}

	if(dev == NULL)
	{
		printf("\nInvalid id");
		return;
	}

	if(!dev->new)
	{
		struct fdisk_command *cmd = (struct fdisk_command*)malloc(sizeof(struct fdisk_command));

		cmd->command = FDISK_REMOVE;	
		cmd->deviceid = dev->id;
		cmd->intid = dev->intid;

		add_tail(&commands, cmd);
	}

	/* Remove device from list */
	dev->removed = 1;

	remove_at(&devices, cit);
}

int apply_changes()
{
	/* go through commands list and apply changes 
	to logic devices :O */
	CPOSITION it = get_head_position(&commands);

	while(it != NULL)
	{
		struct fdisk_command *cmd = (struct fdisk_command*)get_next(&it);

		if(cmd->command == FDISK_CREATE)
		{
			printf("\nCreating partition: slba=%i, size=%i, type=%x ... ", cmd->slba, cmd->size, cmd->type);
			if(apply_create(cmd))
			{
				printf("FAILED.");
				return 1;
			}
			printf("OK.");
		}
		else
		{
			printf("\nRemoving partition: id=%x ... ", cmd->intid);
			if(apply_remove(cmd))
			{
				printf("FAILED.");
				return 1;
			}
			printf("OK.");
		}
	}

	printf("\nChanges applied\n");

	remove_all(&commands);
	
	/* Rebuild partitions list */
	build_partition_list();

	return 0;
}

int apply_create(struct fdisk_command *cmd)
{
	struct atac_create_ldev_param params;
	struct atac_ioctl_createldev iocmd;
	struct stddev_ioctrl_res res;
	int taskid;

	params.ptype = cmd->type;
	params.bootable = 0;
	params.start_lba = cmd->slba;
	params.size = cmd->size;

	/* Send create logic device IOCTRL to the ata controler */
	iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_CREATELDEV;
	iocmd.create_dev_smo = share_mem(aid, &params, sizeof(struct atac_create_ldev_param), READ_PERM | WRITE_PERM);
	iocmd.msg_id = 0;
	iocmd.ret_port = 7;
	iocmd.logic_deviceid = cmd->deviceid;

	send_msg(aid, stddevport, &iocmd);

	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &res, &taskid);

	claim_mem(iocmd.create_dev_smo);

	if(res.ret == STDDEV_ERR)
	{
		return 1;
	}

	return 0;
}

int apply_remove(struct fdisk_command *cmd)
{
	/* Send remove logic device IOCTRL to the ata controler */
	struct atac_ioctl_removeldev iocmd;
	struct stddev_ioctrl_res res;
	int taskid;

	/* Send create logic device IOCTRL to the ata controler */
	iocmd.command = STDDEV_IOCTL;
	iocmd.request = ATAC_IOCTRL_REMOVELDEV;
	iocmd.msg_id = 0;
	iocmd.ret_port = 7;
	iocmd.logic_deviceid = cmd->deviceid;

	send_msg(aid, stddevport, &iocmd);

	while(get_msg_count(7) == 0){ reschedule(); }

	get_msg(7, &res, &taskid);

	if(res.ret == STDDEV_ERR)
	{
		return 1;
	}

	return 0;
}

void sort_list()
{
	/* FIXME: This should sort the devices list by physical and logical device id. */
}


