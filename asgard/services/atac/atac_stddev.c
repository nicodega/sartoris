/*
*
*	IOCTRL operations allowed for hdd driver.
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*	Matias Brunstein Macri	mbmacri@dc.uba.ar
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

#include "atac.h"

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
	struct stdservice_query_res qres;

	qres.command = STDSERVICE_QUERYINTERFACE;
	qres.ret = STDSERVICE_RESPONSE_FAILED;
	qres.msg_id = query_cmd->msg_id;

	switch(query_cmd->uiid)
	{
		case STDDEV_UIID:
			// check version
			if(query_cmd->ver != STDDEV_VERSION) break;
			qres.port = STDDEV_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		case STD_BLOCK_DEV_UIID:
			if(query_cmd->ver != STD_BLOCK_DEV_VER) break;
			qres.port = STDDEV_BLOCK_DEV_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}
struct stddev_res process_stddev(struct stddev_cmd stddev_msg, int sender_id)
{
	struct stddev_res res;
	struct stddev_devtype_res *type_res;
	struct stddev_ver_res *ver_res;
	int i = 0, owners, exclusive, ownerid;
	
	res.logic_deviceid = -1;
	res.command = stddev_msg.command;

	switch(stddev_msg.command)
	{
		case STDDEV_GET_DEVICETYPE:
			type_res = (struct stddev_devtype_res*)&res;
			type_res->dev_type = STDDEV_BLOCK;
			type_res->ret = STDDEV_OK;
			type_res->msg_id = stddev_msg.msg_id;
			type_res->block_size = 512;
			return *((struct stddev_res*)type_res);
		case STDDEV_VER:
			ver_res = (struct stddev_ver_res*)&res;
			ver_res->ret = STDDEV_OK;
			ver_res->ver = STDDEV_VERSION;
			ver_res->msg_id = stddev_msg.msg_id;
			return *((struct stddev_res*)ver_res);
		case STDDEV_GET_DEVICE:
			res.logic_deviceid = stddev_msg.padding0;
			// if logic id is physical then 
			// any logic devices cant be exclusive
			// if its a logic device no logic dev can be exclusive
			if(ldev_stdget(res.logic_deviceid, sender_id, 1))
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
			}
			break;
		case STDDEV_GET_DEVICEX:
			res.logic_deviceid = stddev_msg.padding0;
			if(ldev_stdget(res.logic_deviceid, sender_id, 0))
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
			}
			break;
		case STDDEV_FREE_DEVICE:
			res.logic_deviceid = stddev_msg.padding0;
			if(ldev_stdfree(res.logic_deviceid, sender_id))
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
			}
			break;
		case STDDEV_IOCTL:
			/* process IOCTRL commands */
			res.logic_deviceid = stddev_msg.padding0;
			res.ret = STDDEV_ERR;

			process_ioctrl((struct stddev_ioctl_cmd *)&stddev_msg, sender_id, ((struct stddev_ioctrl_res*)&res));
			break;
		default:
			res.msg_id = stddev_msg.msg_id;
			res.ret = STDDEV_ERR;
			break;
	}

	return res;
}

int check_ownership(int task, int logic_device)
{
	return check_ownershipex(task, logic_device, 0);
}

int check_ownershipex(int task, int logic_device, int exclusive)
{
	struct logic_device *ld;

	ld = ldev_get(logic_device);

	if(ld == NULL) return 0;

	int i = 0;

	while(i < ld->owners)
	{
		if(ld->ownerid[i] == task)
		{
			return (!exclusive || ld->exclusive);
		}
	}

	return 0;
}

