#include "fdc_internals.h"

/* disk geometry */

int heads = DG144_HEADS;
int cyl = DG144_TRACKS;
int sect_per_cyl = DG144_SPT;
int gap3 = DG144_GAP3RW;

/* status */

int int_ack = 0;
int statusz = 0;
char status[7];

/* response regs */

char sr0;
char fdc_track = -1;

/* time vars */

long motor_tout = -1, clockv = 0;

/* motor enabled */

int motor = 0;

/* DMA selected op */

int dma_op = WRITE;

int dma_smo = -1;

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

char buffer[512];

/* functions */

void init_drive() {

	int die = 0,id_proc, result,i=0;
	struct fdc_command fdc_msg;
	struct fdc_response fdc_res;
	struct thread thr;
	struct dma_command dma_msg;
	struct dma_response dma_res;
	
	__asm__ ("sti"::);

	init_rtc(0x0c);	/* initialize RTC */

    open_port(FDC_COMMAND_PORT, 0, UNRESTRICTED);
    open_port(FDC_DMA_TRANSACTIONS, 0, UNRESTRICTED);

	/* create timer thread */

	thr.task_num=FDC_TASK;
	thr.invoke_mode=PRIV_LEVEL_ONLY;                      
	thr.invoke_level=0;
	thr.ep= (void*)&timer;
	thr.stack= (void*)(0xe000 - 0x04);

	create_thread(TIMER_THR, &thr);
	create_int_handler(40, TIMER_THR, true, 5);
	/* IRQ8 -> 40 in the IDT */
	
	thr.ep= (void*)&irq_handler;
	thr.stack= (void*)(0xe000 - 0x2F);

	create_thread(FDCI_THR, &thr);
	create_int_handler(38, FDCI_THR, true, 6);
	/* IRQ6 -> 38 in the IDT */

	/* get DMA channel 2 from DMA manager */

	dma_msg.op = GET_CHANNEL;
	dma_msg.ret_port = FDC_DMA_TRANSACTIONS;
	dma_msg.channel = 2;
	dma_msg.channel_mode = SINGLE_TRANSFER | AUTO_INIT | WRITE;
	dma_msg.buff_size = 512;

	send_msg(DMA_MAN_TASK, DMA_COMMAND_PORT, &dma_msg);

        while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) {
	  run_thread(SCHED_THR);
	}

	get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);

	if (dma_res.result != DMA_OK) {
	  reset();
	  destroy_thread(TIMER_THR);
	  destroy_thread(FDCI_THR);
	  while (1);   /* failed to initialize */
	}

	dma_smo = dma_res.res1;   /* store the DMA buffer smo returned bi the DMA manager */

	reset();

	/* now we enter an infinite loop, checking and procesing
	   messages */

	while (!die) {
	
	  /* wait for a message to serve */
	  
	  while (get_msg_count(FDC_COMMAND_PORT) == 0) { 
	    run_thread(SCHED_THR);
	  }

	  /* read incoming message */

	  get_msg(FDC_COMMAND_PORT, &fdc_msg, &id_proc);

	  motor_on();
	  if (disk_change()) {
	    reset();
	  }
	  motor_off();
		
	  switch (fdc_msg.op) {
	  case WRITE_BLOCK: 
	    result = write_block(fdc_msg.block, fdc_msg.smoid);
	    break;
	  case READ_BLOCK: 
	    result = read_block(fdc_msg.block, fdc_msg.smoid);
	    break;
	  case FDC_DIE:
	    die = 1;
	    break;
	  default:
	    result = -1;
	  }

	  /* the requested operation was performed
	     prepare outgoing message */
		
		if (!die) {
			fdc_res.op = fdc_msg.op;
			fdc_res.result = result;
			fdc_res.res1 = 0;
			fdc_res.res2 = 0;

			send_msg(id_proc, fdc_msg.ret_port, &fdc_res);
		}

	}

	/* clean up */
		
	reset();
	destroy_thread(TIMER_THR);
	destroy_thread(FDCI_THR);

	/* TO DO: free DMA channel 
	   tell the scheduler to unload us */

	/* not implemented yet */

	while (1); /* wait to be unloaded */
}

int send_byte(int byte) {

	int tout = TIMEOUT;

	tout += clock();

	while (busy()) {
		if (tout < clock()) { return -1; }
	}
	
	send_data(byte);

	return 0;
}

int get_byte() {

	int tout = TIMEOUT;

	tout += clock();

	while (!req()) {
		if(tout < clock()) { return -1; }
	}

	return get_data();
}

void get_result() {

	statusz = 0;

	while (statusz < 7 && on_command()) { 
	  status[statusz++] = get_byte(); 
	}

}

void sense_int() {

        /* send a "sense interrupt status" command */

	send_byte(CMD_SENSEI);

	sr0 = get_byte();
	fdc_track = get_byte();

}

void reset() {
	
	int tout = 32;

	set_floppy(DMA_ENABLE); /* this resets the fdc and turns the motor off */
	motor = 0;

	set_data_rate(0);	/* 512 */

	set_floppy(DMA_ENABLE | DRIVE_ENABLE);

	/* at this point an interrupt is triggered
	   because of the reset procedure... */

	tout += clock();

	while (!int_ack){
		if (tout < clock()) {
		      return; /* failed */ 
		}
		run_thread(SCHED_THR);
	}

	int_ack = 0;
	sense_int();

	/* specify drive timings */

	send_byte(CMD_SPECIFY);
 	send_byte(0xdf);	/* SRT = 3ms, HUT = 240ms */
	send_byte(0x02);	/* HLT = 16ms, ND = 0 */
 	
	seek(1);
 
	recalibrate();

}

void convert_from_LBA(int block, int *head, int *track, int *sector) {

	*track = block / (sect_per_cyl * heads);
	*head = (block % (sect_per_cyl * heads)) / sect_per_cyl;
	*sector = (block % sect_per_cyl) + 1;

}

void motor_on() {

	int tout = 8;

	motor_tout = -1; /* finish countdown */
	
	if (motor) { return; }

	/* turn on the motor and wait for 8 ticks (0.5 s aprox) */

	set_floppy(DMA_ENABLE | MOTOR_ON | DRIVE_ENABLE);   /* motor on, DMA enable, drive enable. */

	tout += clock();

	while (tout > clock()) { run_thread(SCHED_THR); }

	motor = 1;
}

void motor_off() {

  motor_tout = 48; /* 3 sec */

}

void recalibrate() {

        int tout = 64;	/* 4 sec */
	int i = 2;

	while (i > 0) {
	
		motor_on();
		
		send_byte(CMD_RECAL);
		send_byte(0);

		tout += clock();

		while (!int_ack) {
			if(tout < clock()) {
				break;
			}
			run_thread(SCHED_THR);
		}

		if (int_ack) {
		        int_ack = 0;	/* clear int flag */
			sense_int();
			motor_off();
			return;
		}
		i--;
	}	

	motor_off();

}

int seek(int track) {

	int tout = 64;

	if (track == fdc_track) { return 0; } /* already there */

	motor_on();

	send_byte(CMD_SEEK);
	send_byte(0);
	send_byte(track);

	tout += clock();

	while (!int_ack) {
			if (tout < clock()) { return -1; }
			run_thread(SCHED_THR);
	}

	int_ack = 0;	/* clear int flag */
	sense_int();

	/* rest for 15 ms to let the head settle (useful on writes) */
	
	tout = 3 + clock();
	
	while (tout > clock());

	if ((sr0 != 0x20) || (track != fdc_track)) {
		motor_off();
		return -1;
	}

	motor_off();

	return 0;
}

int read_block(int block, int smo_id_dest) {
	return fdc_rw(block, smo_id_dest, 0);
}

int write_block(int block, int smo_id_src) {
	return fdc_rw(block, smo_id_src, 1);
}

int fdc_rw(int block, int smo_id, int write) {

	int head, track, sector, i = 3, tout;
	struct dma_response dma_res;
	int id_proc;

	/* convert logical address into physical address */

	convert_from_LBA(block, &head, &track, &sector);

	/* if it's a write operation, we must copy the data
	   to the dma buffer */

	if (write) {
	  /* copy data from SMO */

		if(smo_id == -1) { return -1; }

		read_mem(smo_id, 0,128, buffer);
	}

	while (i != 0) {
	
		motor_on();

		if (disk_change()) {
			seek(1);
			recalibrate();
			motor_off();
			return -1;
		}

		if (seek(track) != 0) {
			motor_off();
			return -1;
		}
	
		motor_on(); /* seek turns it off */

		set_data_rate(0);

		/* initialize DMA for transfer */

		if (!write) {
		  /* read operation */
		 
		  if (dma_op != WRITE) {
		    send_msg(DMA_MAN_TASK, DMA_COMMAND_PORT, &prepare_read);
		    while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) { 
		      run_thread(SCHED_THR); 
		    }
		    get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);
		    if(dma_res.result != DMA_OK) {
		      return -1;
		    }
		    dma_smo = dma_res.res1;
		    dma_op = WRITE;
		  }
		  
		  send_byte(CMD_READ);
		} else {
		  if (dma_op != READ) {
		    send_msg(DMA_MAN_TASK, DMA_COMMAND_PORT, &prepare_write);
		    while (get_msg_count(FDC_DMA_TRANSACTIONS) == 0) { 
		      run_thread(SCHED_THR);
		    }
		    get_msg(FDC_DMA_TRANSACTIONS, &dma_res, &id_proc);
		    if (dma_res.result != DMA_OK) { return -1; }
		    dma_smo = dma_res.res1;
		    dma_op = READ;
		  }
		  write_mem(dma_smo, 0, 128, buffer);
		  send_byte(CMD_WRITE);
		}

		send_byte(head);
		send_byte(track);
		send_byte(head);
		send_byte(sector);
		send_byte(2);	/* 512 bytes/sector */
		send_byte(sect_per_cyl);
		send_byte(gap3);
		send_byte(0xff);

		/* now wait for command completion */

		tout = 128 + clock();

		while (!int_ack) {
			if (tout < clock()) {
				motor_off();
				return -1;
			}
			run_thread(SCHED_THR);
		}

		int_ack = 0;
		get_result();

		/* check results */
		if (((status[0] & 0xc0) == 0) && status[5] > sector) {
			motor_off();
			break;
		}

		recalibrate();
		i--;

		if (i == 0) {
			motor_off();	
			return -1;
		}
	}

	/* if it was a read op then copy the data
	   from the buffer */
	
	if (!write) {
		if (smo_id == -1) {
			return -1;
		}	
		read_mem(dma_smo,0,128,buffer);
		write_mem(smo_id,0,128,buffer);
	}

	motor_off();

	return 0;
}

void irq_handler() {

	while(1) {
		__asm__	 __volatile__ ("movl $0x20, %%eax;"
			               "outb %%al, $0x20" : : :"eax");

		int_ack = 1;
		      
		ret_from_int();
	}
}

void timer() {

	while(1) {
		
	        /* send EOI to master and slave pics */
		__asm__ __volatile__ ("movb $0x20, %%al;" 
			              "outb %%al, $0xa0;" 
			              "outb %%al, $0x20" : : : "eax");
	
		clockv++;
		
		if (motor != 0 && motor_tout != -1) {
			motor_tout--;
			if (motor_tout == 0) {
			  /* turn off the motor */
			  set_floppy(DMA_ENABLE | DRIVE_ENABLE);
			  motor_tout = -1; 
			  motor = 0;
			}
		}

		/* read RTC status C so we can continue... */

		read_rtc_status();

		ret_from_int();
	}
}

int clock(){

	return clockv;
}
