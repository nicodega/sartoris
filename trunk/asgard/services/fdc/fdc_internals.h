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

#ifndef FDC_INT
#define FDC_INT

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <drivers/floppy/floppy.h>
#include <services/dma_man/dma_man.h>
#include <drivers/rtc/rtc.h>
#include <os/pman_task.h>
#include <services/stds/stdservice.h>
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/directory/directory.h>
#include <services/pmanager/services.h>
#include <lib/scheduler.h>

///// DEFINES /////

#define FDC_STDDEVMAX 10
#define MAX_OWNERS 10

/* PORTS */
#define STDDEV_PORT				1
#define STDDEV_BLOCK_DEV_PORT	2
#define FDC_DMA_TRANSACTIONS	3		// por used for DMA transactions
#define FDC_PMAN_PORT			4
	
/* drive geometries */
#define DG144_HEADS       2     /* heads per drive (1.44M) */
#define DG144_TRACKS     80     /* number of tracks (1.44M) */
#define DG144_SPT        18     /* sectors per track (1.44M) */
#define DG144_GAP3FMT  0x54     /* gap3 while formatting (1.44M) */
#define DG144_GAP3RW   0x1b     /* gap3 while reading/writing (1.44M) */

#define DG168_HEADS       2     /* heads per drive (1.68M) */
#define DG168_TRACKS     80     /* number of tracks (1.68M) */
#define DG168_SPT        21     /* sectors per track (1.68M) */
#define DG168_GAP3FMT  0x0c     /* gap3 while formatting (1.68M) */
#define DG168_GAP3RW   0x1c     /* gap3 while reading/writing (1.68M) */
	
;/* command bytes */
#define CMD_SPECIFY (0x03)  /* specify drive timings */
#define CMD_WRITE   (0xc5)  /* write data (MT,MFM) */
#define CMD_READ    (0xe6)  /* read data (MT,SK,MFM) */
#define CMD_RECAL   (0x07)  /* recalibrate */
#define CMD_SENSEI  (0x08)  /* sense interrupt status */
#define CMD_FORMAT  (0x4d)  /* format track (+ MFM) */
#define CMD_SEEK    (0x0f)  /* seek track */
#define CMD_VERSION (0x10)  /* FDC version */

; ///// TIMEOUT /////

#define TIMEOUT 64
#define INT_TIMEOUT 64
#define MOTOR_TIMEOUT 32
#define RW_TRIES 3

/////// RET VALUES /////
#define FDC_OK		0
#define FDC_TIMEOUT	1
#define FDC_ERR		-1


#define RESET_SEEK 5
///// FUNCTIONS /////

int send_byte(int byte);
int get_byte();
void irq_handler();
void sense_int();
void reset();
void convert_from_LBA(int block, int *head, int *track, int *sector);
void motor_on();
void motor_off();
void recalibrate();
int seek(int track);
int read_block(int block, int smo_id_dest);
int write_block(int block, int smo_id_src);
int fdc_rw(int block, int smo_id, int offset, int write);

void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
struct stddev_res process_stddev(struct stddev_cmd stddev_msg, int sender_id);
void swap_array(int *array, int start, int length);
int check_ownership(int process_id, int logic_device);
int mwrite_block(struct stdblockdev_writem_cmd *cmd);
int mread_block(struct stdblockdev_readm_cmd *cmd);

int wait_fdc();
void get_result();
void reset_drive();

// timer functions //

void timer();
int clock();

#endif

