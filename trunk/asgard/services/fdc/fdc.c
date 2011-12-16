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

#include "fdc_internals.h"

#include <lib/debug.h>
#include <lib/wait_msg_sync.h>

struct stdservice_res dieres;
int dieid;
int dieretport;

/* disk geometry */

static int heads = DG144_HEADS;
static int cyl = DG144_TRACKS;
static int sect_per_cyl = DG144_SPT;
static int gap3 = DG144_GAP3RW;
 
/* status */

static int int_ack = 0;
static int statusz = 0;
static char status[7];

/* response regs */

static char sr0;
static char fdc_track = -1;

/* time vars */

volatile long motor_tout = -1;
volatile unsigned long long clockv = 0;
volatile unsigned long long tout;

/* motor enabled */

static int motor = 0;

/* DMA selected op */

static int dma_op = WRITE;

static int dma_smo = -1;

/* DMA channel messages */

struct dma_command prepare_read = { SET_CHANNEL, 
				    FDC_DMA_TRANSACTIONS,
				    2, 
				    AUTO_INIT | WRITE | SINGLE_TRANSFER, 
				    0,
				    0,
				    512 };

struct dma_command prepare_write = { SET_CHANNEL,
				     FDC_DMA_TRANSACTIONS,
				     2,
				     AUTO_INIT | READ | SINGLE_TRANSFER, 
				     0,
				     0,
				     512 };

/* intermediate buffer */

__attribute__ ((aligned (4096))) static unsigned char buffer[512];
static unsigned char stacks[2][512];

static int owners = 0;
static int exclusive = -1;
static int ownerid[MAX_OWNERS] = {-1,}; // array of device owners
static int dma_man_task;
static int timer_thr;
static int fdc_thr;

void mem_copy(unsigned char *source, unsigned char *dest, int count)
{
	int i = count;
	
	while(i != 0){
		dest[i] = source[i];
		i--;
	}
}

void destroy_threads()
{
	struct pm_msg_destroy_thread msg_destroy;
	struct pm_msg_response      msg_res;
	int sender_id;

	msg_destroy.pm_type = PM_DESTROY_THREAD;
	msg_destroy.req_id = 0;
	msg_destroy.response_port = FDC_PMAN_PORT;
	msg_destroy.thread_id = timer_thr;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_destroy);

	while (get_msg_count(FDC_PMAN_PORT) == 0) reschedule();

	get_msg(FDC_PMAN_PORT, &msg_res, &sender_id);

	msg_destroy.pm_type = PM_DESTROY_THREAD;
	msg_destroy.req_id = 0;
	msg_destroy.response_port = FDC_PMAN_PORT;
	msg_destroy.thread_id = fdc_thr;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_destroy);

	while (get_msg_count(FDC_PMAN_PORT) == 0) reschedule();

	get_msg(FDC_PMAN_PORT, &msg_res, &sender_id);
}

/* functions */
void create_timer_thread()
{
	struct pm_msg_create_thread msg_create_thr;
	struct pm_msg_response      msg_res;
	int sender_id;

	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = 0;
	msg_create_thr.response_port = FDC_PMAN_PORT;
	msg_create_thr.task_id = get_current_task();
    msg_create_thr.stack_addr = NULL;
	msg_create_thr.entry_point = timer;
	msg_create_thr.interrupt = 40;
	msg_create_thr.int_priority = 5;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	while (get_msg_count(FDC_PMAN_PORT) == 0) reschedule();

	get_msg(FDC_PMAN_PORT, &msg_res, &sender_id);

	timer_thr = msg_res.new_id;
}

void create_fdc_thread()
{
	struct pm_msg_create_thread msg_create_thr;
	struct pm_msg_response      msg_res;
	int sender_id;

	msg_create_thr.pm_type = PM_CREATE_THREAD;
	msg_create_thr.req_id = 0;
	msg_create_thr.response_port = FDC_PMAN_PORT;
	msg_create_thr.task_id = get_current_task();
	msg_create_thr.stack_addr = NULL;
	msg_create_thr.entry_point = irq_handler;
	msg_create_thr.interrupt = 38;
	msg_create_thr.int_priority = 6;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &msg_create_thr);

	while (get_msg_count(FDC_PMAN_PORT) == 0) reschedule();

	get_msg(FDC_PMAN_PORT, &msg_res, &sender_id);

	fdc_thr = msg_res.new_id;
}

void get_dma_man_task()
{
	char *service_name = "system/dmac";
	struct directory_resolveid resolve_cmd;
	struct directory_response dir_res;
	int sender_id;

	// resolve dma service //
	resolve_cmd.command = DIRECTORY_RESOLVEID;
	resolve_cmd.ret_port = FDC_PMAN_PORT;
	resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);
	resolve_cmd.thr_id = get_current_thread();
    
	dma_man_task = -1;
    
    do
    {
	    while(send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &resolve_cmd) < 0) 
        { 
            reschedule(); 
        }

        do
        {
	        while (get_msg_count(FDC_PMAN_PORT) == 0) reschedule();

	        get_msg(FDC_PMAN_PORT, &dir_res, &sender_id);

        }while(sender_id != DIRECTORY_TASK);

    }while(dir_res.ret != DIRECTORYERR_OK);

	claim_mem(resolve_cmd.service_name_smo);

    dma_man_task = dir_res.ret_value;
}

void init_drive() 
{
	int die = 0, id_proc = -1, result, i = 0, res;
	struct thread thr;
	struct dma_command dma_msg;
	struct dma_response dma_res;
	struct stddev_cmd stddev_msg;
	struct stdblockdev_cmd fdc_msg;
	struct stdblockdev_res fdc_res;
	struct stdservice_cmd service_cmd;
	int service_count, stddev_count, stdblockdev_count;
	char *service_name = "devices/fdc";
	struct directory_register reg_cmd;
	struct directory_response dir_res;
	struct blockdev_info0 devinf;
	struct stdblockdev_devinfo_cmd *dinfmsg;
	struct stdservice_res servres;
	
	init_rtc(0x0c);	/* initialize RTC at 16 ints x sec */

    // open ports with permisions for services only (lv 0, 1, 2) //
    open_port(FDC_PMAN_PORT, 2, PRIV_LEVEL_ONLY);
	open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDDEV_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(STDDEV_BLOCK_DEV_PORT, 2, PRIV_LEVEL_ONLY);
    open_port(FDC_DMA_TRANSACTIONS, 2, PRIV_LEVEL_ONLY);

	__asm__ __volatile__ ("sti"::);

	/* create timer thread */
	create_timer_thread();

	/* Create floppy controler thread */
	create_fdc_thread();

	/* Get DMA MAN task from directory */
	get_dma_man_task();

	/* get DMA channel 2 from DMA manager */
	dma_msg.op = GET_CHANNEL;
	dma_msg.ret_port = FDC_DMA_TRANSACTIONS;
	dma_msg.channel = 2;
	dma_msg.channel_mode = SINGLE_TRANSFER | AUTO_INIT | WRITE;
	dma_msg.buff_size = 512;

	send_msg(dma_man_task, DMA_COMMAND_PORT, &dma_msg);

    while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) { reschedule(); }

	get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);

	if (dma_res.result != DMA_OK) 
	{
        __asm__ __volatile__ ("cli"::);
        string_print("FDC DMA GET FAILED",23*160,12);
        reset();
        destroy_threads();
        for(;;);   /* failed to initialize */
	}

	dma_smo = dma_res.res1;   /* store the DMA buffer smo returned by the DMA manager */

	devinf.max_lba = 2879;
	devinf.media_size = 1474048;
	devinf.metadata_lba_end = 0;
	
    // register service with directory //
	reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
	reg_cmd.ret_port = 1;
	reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);

	send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd);

	while (get_msg_count(1) == 0) { reschedule(); }

	get_msg(1, &dir_res, &id_proc);

	claim_mem(reg_cmd.service_name_smo);
    
    /* prepare the drive*/

	// reset fdc
	reset();
    
	// place the drive on a known state
	reset_drive();
    
	/* now we enter an infinite loop, checking and procesing
	   messages */
    int k = 7;
    int ports[] = {STDSERVICE_PORT, STDDEV_PORT, STDDEV_BLOCK_DEV_PORT};
    int counts[3];
    unsigned int mask = 0x7;
    string_print("FDC ALIVE",3*160 - 18,k++);
	while(!die)
	{		
        while(wait_for_msgs_masked(ports, counts, 3, mask) == 0){}
        string_print("FDC ALIVE",3*160 - 18,k++);
		
		/* process stdservice commands */
		service_count = counts[0];
		
		while(service_count != 0)
		{
			get_msg(STDSERVICE_PORT, &service_cmd, &id_proc);

			servres.ret = STDSERVICE_RESPONSE_OK;
			servres.command = service_cmd.command;

			if(service_cmd.command == STDSERVICE_DIE)
			{
				die = 1;
				dieres.ret = STDSERVICE_RESPONSE_OK;
				dieres.command = service_cmd.command;
				dieid = id_proc;
				dieretport = service_cmd.ret_port;
				break;
			}
			else if(service_cmd.command == STDSERVICE_QUERYINTERFACE)
			{
				process_query_interface((struct stdservice_query_interface *)&service_cmd, id_proc);
				service_count--;
				continue;
			}
			send_msg(id_proc, service_cmd.ret_port, &servres);
			service_count--;
		}

		stddev_count = counts[1];

		/* process stddev messages */
		while(!die && stddev_count != 0)
		{
			get_msg(STDDEV_PORT, &stddev_msg, &id_proc);

			struct stddev_res res = process_stddev(stddev_msg, id_proc);

			send_msg(id_proc, stddev_msg.ret_port, &res);

			stddev_count--;
		}

		stdblockdev_count = counts[2];

		/* process stdblock_dev msgs */
		while(!die && stdblockdev_count != 0)
		{			
			get_msg(STDDEV_BLOCK_DEV_PORT, &fdc_msg, &id_proc);

			// if this is a physical id, set devinf.metadata_lba_end = 0;
			if(fdc_msg.dev & STDDEV_PHDEV_FLAG)
			{
				devinf.metadata_lba_end = 0; // allow access to all sectors
			}
			else
			{
				devinf.metadata_lba_end = 1; // ignore bootsector
			}

			fdc_msg.dev &= !STDDEV_PHDEV_FLAG; // tread everything as a physical device

			// check ownership for command processing
			if(!die && fdc_msg.command == BLOCK_STDDEV_INFO)
			{
				// fill struct
				dinfmsg = (struct stdblockdev_devinfo_cmd *)&fdc_msg;
				write_mem(dinfmsg->devinf_smo, 0, sizeof(struct blockdev_info0), &devinf);

				// send ok response
				fdc_res.command = fdc_msg.command;
				fdc_res.dev = (unsigned char)fdc_msg.dev; 
				fdc_res.msg_id = fdc_msg.msg_id;
				fdc_res.ret = STDBLOCKDEV_OK;
				send_msg(id_proc, fdc_msg.ret_port, &fdc_res);
				stdblockdev_count--;
				continue;
			}
			else if(!die && (!check_ownership(id_proc, fdc_msg.dev) || fdc_msg.dev != 0 ))
			{
				fdc_res.command = fdc_msg.command;
				fdc_res.dev = (unsigned char)fdc_msg.dev; 
				fdc_res.msg_id = fdc_msg.msg_id;
				fdc_res.ret = STDBLOCKDEV_ERR;
				if(fdc_msg.dev != 0)
				{
					fdc_res.extended_error = STDBLOCKDEVERR_WRONG_LOGIC_DEVICE;
				}
				else
				{
					fdc_res.extended_error = -1;
				}
				send_msg(id_proc, fdc_msg.ret_port, &fdc_res);
				stdblockdev_count--;
				continue;
			}

			if (disk_changed()) {
				reset();
				reset_drive();
			}
			
			// fdc driver will consider only pysical devices.
			switch (fdc_msg.command) {
			case BLOCK_STDDEV_WRITE: 
				result = write_block(fdc_msg.pos, fdc_msg.buffer_smo);
				break;
			case BLOCK_STDDEV_READ: 
				result = read_block(fdc_msg.pos, fdc_msg.buffer_smo);
				break;
			case BLOCK_STDDEV_WRITEM: 
				result = mwrite_block((struct stdblockdev_writem_cmd*)&fdc_msg);
				break;
			case BLOCK_STDDEV_READM: 
				result = mread_block((struct stdblockdev_readm_cmd*)&fdc_msg);
				break;
			default:
				result = FDC_ERR;
			}
			
			fdc_res.command = fdc_msg.command;
			fdc_res.dev = (unsigned char)fdc_msg.dev;
			fdc_res.msg_id = fdc_msg.msg_id;

			if(result != FDC_OK)
			{
				fdc_res.ret = STDBLOCKDEV_ERR;
			}
			else
			{
				fdc_res.ret = STDBLOCKDEV_OK;
			}
			
			send_msg(id_proc, fdc_msg.ret_port, &fdc_res);
			
			stdblockdev_count--;
		}
		
	}

	/* clean up */
		
	reset();
	reset_drive();

	destroy_threads();

	/* free DMA channel */
	dma_msg.op = FREE_CHANNEL;
	dma_msg.ret_port = FDC_DMA_TRANSACTIONS;
	dma_msg.channel = 2;

	send_msg(dma_man_task, DMA_COMMAND_PORT, &dma_msg);

    while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) { reschedule(); }

    close_port(FDC_PMAN_PORT);
	close_port(STDSERVICE_PORT);
    close_port(STDDEV_PORT);
    close_port(STDDEV_BLOCK_DEV_PORT);

	/* tell the scheduler to unload us */
	send_msg(dieid, dieretport, &dieres);

	for(;;); /* wait to be unloaded */
}

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
	int i = 0;

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
			if(exclusive == -1)
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
				ownerid[owners] = sender_id;
				owners++;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			break;
		case STDDEV_GET_DEVICEX:
			res.logic_deviceid = stddev_msg.padding0;
			if(owners == 0)
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_OK;
				ownerid[owners] = sender_id;
				owners++;
				exclusive = 0;
			}
			else
			{
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			break;
		case STDDEV_FREE_DEVICE:
			res.logic_deviceid = stddev_msg.padding0;
			if(exclusive == -1)
			{
				// non exclusive free
				while(i < owners)
				{
					if(ownerid[i] == sender_id)
					{
						res.msg_id = stddev_msg.msg_id;
						res.ret = STDDEV_OK;
						owners--;
						swap_array(ownerid, i, owners);
						return res;
					}
					i++;
				}
				res.msg_id = stddev_msg.msg_id;
				res.ret = STDDEV_ERR;
			}
			else
			{
				// exclusive free
				if(ownerid[0] == sender_id)
				{
					res.msg_id = stddev_msg.msg_id;
					res.ret = STDDEV_OK;
					owners--;
					exclusive = -1;
					return res;
				}
				res.ret = STDDEV_ERR;
			}
			break;
	}

	return res;
}

void swap_array(int *array, int start, int length)
{
	int k = start;

	while(start < length)
	{
		array[k] = array[k + 1];
		k++;
	}

	if(length != 0) array[k] = -1;
	
}

int check_ownership(int process_id, int logic_device)
{
	int i = 0;

	while(i < owners)
	{
		int ow = ownerid[i];
		if(ownerid[i] == process_id) return 1;
		i++;
	}

	return 0;
}

#define SET_TIMEOUT(ticks) tout = ticks + clockv;

// be carefoul with clockv wrapping
#define TIMED_OUT (tout < clockv)

/* wait for interrupt */
int wait_fdc_ex(int seek)
{
	SET_TIMEOUT(INT_TIMEOUT)

	while (!int_ack && !TIMED_OUT) 
    {
		// waiting 
	}

	if(TIMED_OUT && !int_ack) return FDC_TIMEOUT;
	int_ack = 0;

	if(!seek) get_result();

	return FDC_OK;
}

int wait_fdc()
{
	return wait_fdc_ex(0);
}

int read_block(int block, int smo_id_dest) 
{
	return fdc_rw(block, smo_id_dest, 0, 0);
}

int write_block(int block, int smo_id_src) 
{
	return fdc_rw(block, smo_id_src, 0, 1);
}

int mwrite_block(struct stdblockdev_writem_cmd *cmd)
{
	int count = 0, offset = 0;
	while(count < cmd->count)
	{
		if(fdc_rw(cmd->pos, cmd->buffer_smo, offset, 1) != FDC_OK) return FDC_ERR;
		offset += 512;
		count++;
		cmd->pos++;
	}
	return FDC_OK; //ok
}
int mread_block(struct stdblockdev_readm_cmd *cmd)
{
	int count = 0, offset = 0;
	while(count < cmd->count)
	{
		if(fdc_rw(cmd->pos, cmd->buffer_smo, offset, 0) != FDC_OK) return FDC_ERR;
		offset += 512;
		count++;
		cmd->pos++;
	}
	return FDC_OK; //ok
}
/* FDC low level functions */
int send_byte(int byte) 
{

	SET_TIMEOUT(TIMEOUT)

	while (!TIMED_OUT) 
    {
		if (!busy()) 
		{ 
			send_data(byte);
			return FDC_OK; 
		}
	}

	if (!busy()) 
	{ 
		send_data(byte);
		return FDC_OK; 
	}

	return FDC_TIMEOUT;
}

int get_byte() 
{

	SET_TIMEOUT(TIMEOUT)

	while (!TIMED_OUT) 
    {
		if(req()) 
		{ 
			return get_data();
		}
	}

	if(req()) 
	{ 
		return get_data();
	}

	return -1;
}

void get_result() 
{

	statusz = 0;

	while (statusz < 7 && on_command()) 
    {
	    status[statusz++] = get_byte(); 
	}

}

void sense_int() 
{

    /* send a "sense interrupt status" command */

	send_byte(CMD_SENSEI);

	sr0 = get_byte();
	fdc_track = get_byte();

}

// specify drive timings
void specify()
{
	set_data_rate(0);	/* 512 */
	send_byte(CMD_SPECIFY);
 	send_byte(0xdf);	/* SRT = 3ms, HUT = 240ms */
	send_byte(0x02);	/* HLT = 16ms, ND = 0 */
}

void reset() 
{	
	int i = 0;

	set_floppy(0); /* this resets the fdc and turns the motor off, and disables DMA */

	set_floppy(DMA_ENABLE | DRIVE_ENABLE);

	motor = 0;

	/* at this point an interrupt is triggered
	   because of the reset procedure... */

	SET_TIMEOUT(INT_TIMEOUT)

	while (!int_ack){
		if (TIMED_OUT && !int_ack) {
		     return;
		}
	}

	/* reset int flag */
	int_ack = 0;

	 /* FDC specs say to sense interrupt status four times */
	for(i = 0; i < 4; i++)
	{
		sense_int();
	}
}

void reset_drive()
{
	motor_tout = -1; /* finish motor countdown */

	fdc_track = 0;
	sr0 = 0;

	specify();
	motor_on();
	seek(RESET_SEEK);
    recalibrate();
	motor_off();
    
}

void convert_from_LBA(int block, int *head, int *track, int *sector) 
{
	*track = block / (sect_per_cyl * heads);
	*head = (block / sect_per_cyl) % heads;
	*sector = (block % sect_per_cyl) + 1;
}

void motor_on() 
{
	motor_tout = -1; /* finish countdown */

	if (motor) 
    { 
		return; 
	}

	/* turn on the motor and wait for 8 ticks (0.5 s aprox) */

	set_floppy(DMA_ENABLE | MOTOR_ON | DRIVE_ENABLE);   /* motor on, DMA enable, drive enable. */

	// wait for spin up
	SET_TIMEOUT(8) // 0.5 s

	while (!TIMED_OUT) { }

	motor = 1;
}

void motor_off() 
{
	if(!motor) return;

	// set timeout for motor
	motor_tout = 48; /* 3 sec */
}

int disk_changed()
{
	int ret = 0;
	motor_on();
	ret = disk_change();	
	motor_off();
	return ret;
}

// precondition: motor on.
void recalibrate() 
{

    int tries = 13;	// try many times

	while (tries > 0) 
	{
		send_byte(CMD_RECAL);
		send_byte(0);	// 0 = fdc number

		wait_fdc_ex(1); // use this because recalibrate does not have results
		
		sense_int();

		if(sr0 & 0x10) break;
		
		tries--;
	}	
}

// precondition: motor on.
int seek(int track) 
{
	if (track == fdc_track) { return FDC_OK; } /* already there */

	send_byte(CMD_SEEK);
	send_byte(0);		// drive number
	send_byte(track);

	if(wait_fdc_ex(1) == FDC_TIMEOUT) return FDC_TIMEOUT;

	/* rest for 15 ms to let the head settle (useful on writes) */
	
	SET_TIMEOUT(3)

	while (!TIMED_OUT){ }

	sense_int();

	// NOTE: On all examples I've seen
	// sr0 was compared to 0x20... by 
	// reading intel specification for the 
	// controller I realized bit 2 represents
	// current head, and that's why I used
	// sr0 & 0xFB instead of sr0 alone. On the docs
	// it says that bit will be 0 but im not sure
	// botchs does it.. eitherway it's ok.
	if (((sr0 & 0xFB) != 0x20) || (track != fdc_track)) 
    {
		return FDC_ERR;
	}

	return FDC_OK;
}

int fdc_rw(int block, int smo_id, int offset, int write) 
{

	int head, track, sector, tries = 3, tout;
	struct dma_response dma_res;
	int id_proc, i, omgs;

	/* convert logical address into physical address */
	convert_from_LBA(block, &head, &track, &sector);

	/* if it's a write operation, we must copy the data
	   to the dma buffer */

	if (write) 
    {
	    /* copy data from SMO */
		read_mem(smo_id, offset, 512, buffer);
	}

	// wait while floppy is busy
	while(busy());
	while (tries != 0) 
    {
		motor_on();
		specify();	// set data rate

		if (seek(track) != FDC_OK) 
        {
			motor_off();
			return FDC_ERR;
		}
	
		// check there is a disk on the drive
		if(disk_change())
		{
            // disk changed
			return FDC_ERR;
		}

		/* initialize DMA for transfer */

		if (!write) 
        {
            /* read operation */
            if (dma_op != WRITE)
            {
                send_msg(dma_man_task, DMA_COMMAND_PORT, &prepare_read);
                while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) 
                { 
                    reschedule(); 
                }
                get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);
                if(dma_res.result != DMA_OK) 
                    return FDC_ERR;
                dma_smo = dma_res.res1;
                dma_op = WRITE;
            }
            motor_on(); // just in case DMA took too long to answer
            send_byte(CMD_READ);
        } 
        else 
        {
            if (dma_op != READ) 
            {
                send_msg(dma_man_task, DMA_COMMAND_PORT, &prepare_write);
                while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) 
                { 
                    reschedule();
                }
                get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);
                if (dma_res.result != DMA_OK) 
                    return FDC_ERR;
                dma_smo = dma_res.res1;
                dma_op = READ;
            }
            if(write_mem(dma_smo, 0, 512, buffer) == FAILURE)
                print("write mem failed");
            motor_on(); // just in case DMA took too long to answer
            send_byte(CMD_WRITE);
        }

		send_byte(head << 2);
		send_byte(track);
		send_byte(head);
		send_byte(sector);
		send_byte(2);	/* 512 bytes/sector */
		send_byte(sect_per_cyl);
		send_byte(gap3);
		send_byte(0xff);

		/* now wait for command completion */
		if(wait_fdc() != FDC_OK)
		{
			// fail
			reset();
			reset_drive();
			return FDC_TIMEOUT;
		}

		/* check results */
		if (((status[0] & 0xc0) == 0)) 
        {
			break;
		}

		// failed try again
		recalibrate();
		
		tries--;

		if (tries == 0) 
        {
			return FDC_ERR;
		}
	}

	/* if it was a read op then copy the data
	   from the buffer */
	
	if (!write) 
    {
		if (smo_id == -1) 
        {
			return FDC_ERR;
		}	
		read_mem(dma_smo,0,512,buffer);
		write_mem(smo_id, offset, 512, buffer);
	}

	motor_off();

	return FDC_OK;
}

void irq_handler() 
{
	__asm__ ("cli"::);

	for(;;) 
    {
		int_ack = 1;
	
		// send EOI to master PIC
		__asm__	 __volatile__ ("movl $0x20, %%eax;"
			                   "outb %%al, $0x20" : : :"eax");
		ret_from_int();
	}
}

void timer() 
{

    __asm__ ("cli"::);

	for(;;)
    {
		
		clockv++;
		
		if (motor && motor_tout != -1) 
        {
			motor_tout--;
			
			if (motor_tout == 0) 
            {
                // turn off the motor 
                set_floppy(DMA_ENABLE | DRIVE_ENABLE);
                motor_tout = -1; 
                motor = 0;
			}
			
		}
	
		/* read RTC status C so we can continue... */
		read_rtc_status();

		/* send EOI to master and slave pics */
		__asm__ __volatile__ ("movb $0x20, %%al;" 
			                  "outb %%al, $0xa0;" 
			                  "outb %%al, $0x20" : : : "eax");
		
		ret_from_int();
	}
}


