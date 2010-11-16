#ifndef FDC_INT
#define FDC_INT

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <services/fdc/fdc.h>
#include <drivers/floppy/floppy.h>
//#include <drivers/dma/dma.h>
#include <services/dma_man/dma_man.h>
#include <drivers/rtc/rtc.h>
#include <oblivion/layout.h>

///// DEFINES /////

#define FDC_COMMAND_PORT 1	// port used to get messages
	
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

#define TIMEOUT 32
#define MOTOR_TIMEOUT 32
#define RW_TRIES 3

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
int fdc_rw(int block, int smo_id, int write);

// timer functions //

void timer();
int clock();

#endif

