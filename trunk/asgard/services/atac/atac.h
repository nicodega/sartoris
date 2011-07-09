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

#ifndef _ATACH_
#define _ATACH_

/* If this define is commented, ata initial operations (like identify device) 
will be performed with CHS. If not, LBA will be used. */
#define ATAC_LBAOP	
/*
If this define is set, devices will be accessed through an interrupt. Otherwise
PIO will be used.
*/
#define ATA_INTS_ENABLED

#include <lib/debug.h>
#include <lib/sprintf.h>
#include <drivers/screen/screen.h>

// Includes
#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <os/pman_task.h>
#include <lib/scheduler.h>
#include <lib/malloc.h>
#include <lib/structures/list.h>
#include <lib/const.h>
#include <lib/critical_section.h>
#include <lib/signals.h>
#include <services/pmanager/services.h>
#include <services/atac/atac_ioctrl.h>
#include "ata.h"
#include "partitions.h"

//stds
#include <services/stds/stdservice.h>
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/directory/directory.h>

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

/* Command ports */
#define STDDEV_BLOCK_DEV_PORT 	3
#define STDDEV_PORT				4
#define ATAC_THREAD_ACK_PORT	5
#define ATAC_SIGNALS_PORT		6
#define ATAC_DMA_TRANSACT_PORT  7

#define MAX_OWNERS				10
#define QUEUE_TASKHISTORY_LEN	16
#define QUEUE_ITEM_TTL			5
#define QUEUE_REORDER_RANGE		5

extern int dma_man_task;

#define DMA_MAN_TASK dma_man_task

#define INT_STACK_SIZE 512

struct logic_device
{
	int adapter;	// adapter on which the channel is located
	int channel;	// ata channel for the device
	int drive;		// device on the ata channel (0 or 1)

	unsigned int id;		
	int owners;		// current owners
	int exclusive;
	int ownerid[MAX_OWNERS];
	unsigned int slba;	// starting lba for the logic device
	unsigned int elba;	// ending lba for the logic device
	unsigned char ptype;
	int bootable;
	char *volume_type;	// Used for the alt OS specification
	unsigned int metadata_end;	// offset from the begining of the device where metadata ends.

	unsigned int ptlba;	// lba for the containing partition entry
	int ptentry;
};

#define DEVICE_COMMAND_BLOCK    0x1
#define DEVICE_COMMAND_IOCTRL   0x2

struct device_command
{
	int msg[4]; 	                    // message to be executed
	int task;							// task for whom the command is intended.
	struct logic_device *ldev;			// logic device where the command is being performed
    int type;                           // type of command
	int ttl;							// time to live. used to prevent starvation because of reordering.
};


struct command_queue
{
	list queue[2];				// pending command queue for each device
	int next_dev;				// will alternate with 0 or 1. this is to prevent device starvation.
								// If the device on the channel with the same ID as last_dev has
								// something waiting on the queue, it will have priority and be executed.
								// once it's executed, last_dev will switch to the other device.
								// If current device has no commands, the turn will be given to the other device.
	struct mutex queue_mutex;	// mutex for accessing the commands queue	
	int ttl_pos[2];
};

#define COMMAND_TIMEOUT 20000

/* Our custom events */
#define IRQ_EVENT				0x1
#define THREAD_CREATED_EVENT	0x2
#define CHANNEL_COMMAND_EVENT	0x3

// Extended error information returned by
// reg_reset(), reg_non_data_*(), reg_pio_data_in_*(),
// reg_pio_data_out_*(), reg_packet(), dma_isa_*()
// and dma_pci_*() functions.

struct REG_CMD_INFO
{
   // entry type, flag and command code
   unsigned char flg;         // see TRC_FLAG_xxx in ataio.h
   unsigned char ct;          // see TRC_TYPE_xxx in ataio.h
   unsigned char cmd;         // command code
   // before regs
   unsigned short  fr1;         // feature (8 or 16 bits)
   unsigned short  sc1;         // sec cnt (8 or 16 bits)
   unsigned char sn1;         // sec num
   unsigned char cl1;         // cyl low
   unsigned char ch1;         // cyl high
   unsigned char dh1;         // device head
   unsigned char dc1;         // device control
   // after regs
   unsigned char st2;         // status reg
   unsigned char as2;         // alt status reg
   unsigned char er2;         // error reg
   unsigned short sc2;         // sec cnt (8 or 16 bits)
   unsigned char sn2;         // sec num
   unsigned char cl2;         // cyl low
   unsigned char ch2;         // cyl high
   unsigned char dh2;         // device head
   // driver error codes
   unsigned char ec;          // detailed error code
   unsigned char to;          // not zero if time out error
   // additional result info
   long totalBytesXfer;       // total bytes transfered
   long drqPackets;           // number of PIO DRQ packets
   long drqPacketSize;        // number of bytes in current DRQ block
   unsigned int failbits;     // failure bits (protocol errors)
      #define FAILBIT15 0x8000   // extra interrupts detected
      #define FAILBIT14 0x4000
      #define FAILBIT13 0x2000
      #define FAILBIT12 0x1000
      #define FAILBIT11 0x0800
      #define FAILBIT10 0x0400
      #define FAILBIT9  0x0200
      #define FAILBIT8  0x0100   // SC( CD/IO bits) wrong at end of cmd
      #define FAILBIT7  0x0080   // byte count odd at data packet xfer time
      #define FAILBIT6  0x0040   // byte count wrong at data packet xfer time
      #define FAILBIT5  0x0020   // SC (IO bit) wrong at data packet xfer time
      #define FAILBIT4  0x0010   // SC (CD bit) wrong at data packet xfer time
      #define FAILBIT3  0x0008   // byte count wrong at cmd packet xfer time
      #define FAILBIT2  0x0004   // SC wrong at cmd packet xfer time
      #define FAILBIT1  0x0002   // got interrupt before cmd packet xfer
      #define FAILBIT0  0x0001   // slow setting BSY=1 or DRQ=1 after AO cmd
   // sector count, multiple count, and CHS/LBA info
   long ns;                   // number of sectors (sector count)
   int mc;                    // multiple count
   unsigned char lbaSize;     // size of LBA used
      #define LBACHS 0           // last command used CHS
      #define LBA28  28          // last command used 28-bit LBA
      #define LBA32  32          // last command used 32-bit LBA (Packet)
      #define LBA48  48          // last command used 48-bit LBA
      #define LBA64  64          // future use?
   unsigned long lbaLow1;     // lower 32-bits of LBA before
   unsigned long lbaHigh1;    // upper 32-bits of LBA before
   unsigned long lbaLow2;     // lower 32-bits of LBA after
   unsigned long lbaHigh2;    // upper 32-bits of LBA after
} ;


/* This structure contains information on a single ata device (there 
might be up to 2 devices per ata channel on a host adapter) */
struct ata_device
{
	// Configuration data for device 0 and 1
	// returned by the reg_config() function.
	int reg_config_info;
		#define REG_CONFIG_TYPE_NONE  0
		#define REG_CONFIG_TYPE_UNKN  1
		#define REG_CONFIG_TYPE_ATA   2
		#define REG_CONFIG_TYPE_ATAPI 3

	// flag bits for 'incompatible' controllers and devices

	int reg_incompat_flags;   // see #defines...
		#define REG_INCOMPAT_DMA_DELAY   0x0001   // set to 1 for delay before
												// and after stopping the
												// DMA engine
		#define REG_INCOMPAT_DMA_POLL    0x0002   // set to 1 for no polling
											// of Alt Status during
											// DMA transfers

	int mode;	// configuration of the device (how the channel should be configured)
				// ad Multiword DMA is really (REALLY) old and was not supported very well
				// on most systems, I've decided to discontinue it's use.
		#define ATAC_DEVMODE_PIO		1
		#define ATAC_DEVMODE_INT		2
		#define ATAC_DEVMODE_UDMA		4

	int address_mode;	// addressing mode used for the device
		#define ATAC_ADDRESMODE_CHS		0
		#define ATAC_ADDRESMODE_LBA28	1
		#define ATAC_ADDRESMODE_LBA32	2
		#define ATAC_ADDRESMODE_LBA48	3
	
	list logic_devs;					// logic devices on the device
	struct logic_device ldev;			// physical logic device
	list ptables;						// partition tables

	/* Identify drive useful data */
	unsigned long sector_count;	// hoy many sectors do we have
	unsigned int  heads;
	unsigned int  cylinders;
	unsigned int  sect_per_track;

	char serial[21];
	unsigned int rwmultiple_max;	// maximum number of sectors which can be isued for read/write multiple
	int supported_multiword;
		#define DMA_MULTIWORD_NOTSUPPORTED	-1
		#define DMA_MULTIWORD0_SUPPORTED	0
		#define DMA_MULTIWORD1_SUPPORTED	1
		#define DMA_MULTIWORD2_SUPPORTED	2
	int supported_multiword_selected;
		#define DMA_MULTIWORD_NONE			0
		#define DMA_MULTIWORD0_SELECTED		1
		#define DMA_MULTIWORD1_SELECTED		2
		#define DMA_MULTIWORD2_SELECTED		3
	
	unsigned int ata_version;
		#define SUPPORTED_ATA_VER	0x0004	// support from ATA2
};

/* An ata channel, for a host adapter */
struct ata_channel
{
	unsigned int id;	// just for internal use

	int pio_xfer_width;

	unsigned int pio_reg_addrs[10];

	unsigned int pio_base_addr1;		// base IO register (Command block registers)
	unsigned int pio_base_addr2;		// base IO register (Control block registers)
	
	unsigned int pio_linear;			// set to non 0 if the device is mapped on memory (?) (physical pages should be mapped to the service addr space)
	int pio_memory_dt_opt;
		#define PIO_MEMORY_DT_OPT0 0  // use Data reg at offset 0H
		#define PIO_MEMORY_DT_OPT8 1  // use Data reg at offset 8H
		#define PIO_MEMORY_DT_OPTB 2  // use Data reg at offsets 400-7ffH


	int dma_pci_prd_type;				 // type of PRD list to use
		#define PRD_TYPE_SIMPLE  0       // use PRD buffer, simple list
		#define PRD_TYPE_COMPLEX 1       // use PRD buffer, complex list
		#define PRD_TYPE_LARGE   2       // use I/O buffer, overlay I/O buffer

	long dma_pci_largeMaxB;   // max LARGE dma transfer size in bytes
	long dma_pci_largeMaxS;   // max LARGE dma transfer size in sectors
	unsigned long *dma_pci_largePrdBufPtr;  // LARGE PRD buffer ptr
	unsigned char *dma_pci_largeIoBufPtr;   // LARGE PRD I/O address

	unsigned long *dma_pci_prd_ptr;  // current PRD buffer address
	int dma_pci_num_prd;             // current number of PRD entries

	struct REG_CMD_INFO reg_cmd_info;

	// last ATAPI command packet size and data
	int reg_atapi_cp_size;
	unsigned char reg_atapi_cp_data[16];

	// flag to control the ~110ms delay for ATAPI devices,
	// no delay if the flag is zero.

	int reg_atapi_delay_flag;

	// the values in these variables are placed into the Feature,
	// Sector Count, Sector Number and Device/Head register by
	// reg_packet() before the A0H command is issued.  reg_packet()
	// sets these variables to zero before returning.  These variables
	// are initialized to zero.  Only bits 3,2,1,0 of reg_atapi_reg_dh
	// are used.

	unsigned char reg_atapi_reg_fr;
	unsigned char reg_atapi_reg_sc;
	unsigned char reg_atapi_reg_sn;
	unsigned char reg_atapi_reg_dh;

	long reg_slow_xfer_flag;
	void ( * reg_drq_block_call_back ) ( struct ata_channel*, struct REG_CMD_INFO * );
	unsigned char *reg_buffer;
	long reg_buffer_size;

	/* This arrays will hold last values read or written to/from each IO port */
	unsigned char pio_last_write[10];
	unsigned char pio_last_read[10];

	// BMCR/BMIDE Status register I/O address
	unsigned int int_bmcr_addr;
	// BMCR/BMIDE Status register at time of last interrupt
	unsigned char int_bm_status;
	// ATA Status register I/O address
	unsigned int int_ata_addr;
	// ATA Status register at time of last interrupt
	unsigned char int_ata_status;

	int int_use_intr_flag;		// use irqs for this channel
	int irq;					// irq number assigned to the channel
	int int_intr_cntr;			// incremented each time there is an interrupt (this will be used for signaling on param0)
								// check its not equal to SIGNAL_IGNORE_PARAM once incremented.
	int int_intr_flag;          // interrupt thread was created
	int int_thread_created;		// set to 1 when channel interrupt thread has been created.

	struct ata_device devices[2];	// devices on channel 
	int devices_count;

	unsigned char *int_stack;	// stack used for the interrupt service
	unsigned int int_stack_size;

	/* ISA DMA Mode */
	int dma_mode;
		#define DMA_MODE_NONE	0	// dma disabled
		#define DMA_MODE_ISA	1	// use multiword ISA dma
		#define DMA_MODE_UDMA	2	// use Ultra DMA

	/* ISA DMA */
	int dma_channel;      // dma channel number (5, 6 or 7)

	int dmaTCbit;					// terminal count bit status
		#define DMA_TC5 0x02        // values used in dmaTCbit
		#define DMA_TC6 0x04
		#define DMA_TC7 0x08

	int channel_mode;
	
	unsigned long dma_count;  // byte/word count for Multiword DMA transfers
	int dma_man_smo;	      // smo DMA manager service gave us, to it's buffer

	struct command_queue queue;	// a thread safe command queue for the channel
	int thread;					// thread id for the channel working process.
	int initialized;			// set to 1 when the working process for the channel is initialized.
	int data_adapterid;			// set to the data adapter id of the channel.
};

/* This struct will provide information on a single ata host adapter */
struct ata_host_adapter
{
	unsigned int dma_pci_bmcr_reg_addr;   // BMCR/BMIDE base i/o address
	
	// channels
	struct ata_channel channels[2];

	int active;	// initialized to 0. If 1 the adapter is present and configured.
};

#define TRC_FLAG_EMPTY 0
#define TRC_FLAG_SRST  1
#define TRC_FLAG_ATA   2
#define TRC_FLAG_ATAPI 3

// command types (protocol types) used by reg_cmd_info.ct,
// trc_cht_types(), etc.  this is a bit shift value.
// NOTE: also see trc_get_type_name() in ATAIOTRC.C !
#define TRC_TYPE_ALL    16    // trace all cmd types
#define TRC_TYPE_NONE    0    // don't trace any cmd types
#define TRC_TYPE_ADMAI   1    // ATA DMA In
#define TRC_TYPE_ADMAO   2    // ATA DMA Out
#define TRC_TYPE_AND     3    // ATA PIO Non-Data
#define TRC_TYPE_APDI    4    // ATA PIO Data In
#define TRC_TYPE_APDO    5    // ATA PIO Data Out
#define TRC_TYPE_ASR     6    // ATA Soft Reset
#define TRC_TYPE_PDMAI   7    // ATAPI DMA In
#define TRC_TYPE_PDMAO   8    // ATAPI DMA Out
#define TRC_TYPE_PND     9    // ATAPI PIO Non-Data
#define TRC_TYPE_PPDI   10    // ATAPI PIO Data In
#define TRC_TYPE_PPDO   11    // ATAPI PIO Data Out

/* This macro provides a small delay */
#define DELAY400NS  { delay_400(channel); }

void delay_400(struct ata_channel *channel);

/* pio.c */
unsigned char pio_inbyte(struct ata_channel *channel, unsigned int addr );
void pio_outbyte(struct ata_channel *channel, unsigned int addr, unsigned char data );
unsigned int pio_inword(struct ata_channel *channel, unsigned int addr );
void pio_outword(struct ata_channel *channel, unsigned int addr, unsigned int data );
void pio_drq_block_in(struct ata_channel *channel, unsigned int addr_datareg, unsigned int buf_addr, long word_cnt );
void pio_drq_block_out(struct ata_channel *channel, unsigned int addr_datareg, unsigned int buf_addr, long word_cnt );

/* tmr.c */
int tmr_wait_timeout(struct ata_channel *channel);
SIGNALHANDLER tmr_set_timeout_int(struct ata_channel *channel);
SIGNALHANDLER tmr_set_timeout(struct ata_channel *channel);
int tmr_check_timeout(SIGNALHANDLER sigh);
void tmr_discard_timeout(SIGNALHANDLER sigh);

/* ataioint.c */
int int_enable_irq(struct ata_channel *channel);
void int_handler( struct ata_channel *channel );

/* ataioreg.c */
static void reg_wait_poll(struct ata_channel *channel, int waiterror, int pollerror );
int reg_config(struct ata_channel *channel);
int reg_reset(struct ata_channel *channel, int skip_flag, int dev_ret );
static int exec_non_data_cmd( struct ata_channel *channel, int dev );
int reg_non_data_chs( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned short cyl, unsigned short head, unsigned short sect );
int reg_non_data_lba28( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lba );
int reg_non_data_lba48( struct ata_channel *channel,int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lbahi, unsigned long lbalo );
static int exec_pio_data_out_cmd(  struct ata_channel *channel, int dev, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_in_chs( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned short cyl, unsigned short head, unsigned short sect, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_in_lba28( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lba, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_in_lba48( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lbahi, unsigned long lbalo, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_out_chs( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned short cyl, unsigned short head, unsigned short sect, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_out_lba28( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lba, unsigned int address, long num_sect, int multi_cnt );
int reg_pio_data_out_lba48( struct ata_channel *channel, int dev, int cmd, unsigned short fr, unsigned short sc, unsigned long lbahi, unsigned long lbalo, unsigned int address, long num_sect, int multi_cnt );
int reg_packet( struct ata_channel *channel, int dev, unsigned int cpbc, unsigned int cpaddress, int dir, long dpbc, unsigned int dpaddress, unsigned long lba );

/* ataiosub.c*/
void sub_zero_return_data( struct ata_channel *channel );
void sub_setup_command( struct ata_channel *channel );
void sub_trace_command( struct ata_channel *channel );
int sub_select( struct ata_channel *channel, int dev );
void sub_atapi_delay( struct ata_channel *channel, int dev );
void sub_xfer_delay( struct ata_channel *channel );

/* ataioisa.c */
// I decided this won't be supported, for Ultra DMA is now the standard and Multiword modes 1 and 2 also 
// require a PCI controller.
// But well, I ported the code so I leave this functions here even though ataioisa.c won't be debugged.
static void set_up_xfer( struct ata_channel *channel, int dir, long bc, unsigned int address );
static void prog_dma_chan( struct ata_channel *channel, unsigned long count, int mode );
void isa_free_dma_channel(struct ata_channel *channel);
static int chk_cmd_done( struct ata_channel *channel );
int dma_isa_config( struct ata_channel *channel, int chan );
static int exec_isa_ata_cmd( struct ata_channel *channel, int dev, unsigned int address, long numSect );
int dma_isa_chs( struct ata_channel *channel, int dev, int cmd, unsigned int fr, unsigned int sc, unsigned int cyl, unsigned int head, unsigned int sect, unsigned int address, long numSect );
int dma_isa_lba28( struct ata_channel *channel, int dev, int cmd, unsigned int fr, unsigned int sc, unsigned long lba, unsigned int address, long numSect );
int dma_isa_lba48( struct ata_channel *channel, int dev, int cmd, unsigned int fr, unsigned int sc, unsigned long lbahi, unsigned long lbalo, unsigned int address, long numSect );
int dma_isa_packet( struct ata_channel *channel, int dev, unsigned int cpbc, unsigned int cpaddress, int dir, long dpbc, unsigned int dpaddress, unsigned long lba );

/* helpers.c */
int channel_configure(struct ata_channel *channel, int dev);
void convert_from_LBA(struct ata_device *dev, int block, int *head, int *track, int *sector);
int channel_read(struct ata_channel *channel, int dev, unsigned int address, unsigned long long lba, unsigned int count);
int channel_write(struct ata_channel *channel, int dev, unsigned int address, unsigned long long lba, unsigned int count);

/* blockdev.c */
int process_blockdev(struct stdblockdev_cmd *bmsg, int task, struct stdblockdev_res *bres);

/* ata_find.c */
void ata_find();
void setup_channel(struct ata_channel *channel, unsigned int id, int pio_xfer_width,
				   unsigned int base_command_reg, unsigned int base_control_reg,
				   char *buffer, unsigned int buffer_size, int data_adapterid);
void create_channel_wp(struct ata_channel *channel);

/* channelwp.c */
void channel_wp();
int wpread(struct ata_channel *channel, struct logic_device *ldev, struct stdblockdev_cmd *bcmd, int multiple, int count);
int wpwrite(struct ata_channel *channel, struct logic_device *ldev, struct stdblockdev_cmd *bcmd, int multiple, int count);

/* logic_devices.c */
int ldev_valdid(int id);
struct logic_device *ldev_get(int id);
int ldev_stdget(int id, int task, int exclusive);
int ldev_stdfree(int id, int task);
int ldev_find(struct ata_channel *channel, int dev);
void ldev_init(struct ata_channel *channel, int dev);

/* command_queue.c */
void queue_init(struct command_queue *queue);
struct device_command *queue_getnext(struct command_queue *queue, int channelid);
void queue_enqueue(struct ata_channel *channel, int device, struct device_command *cmd);
CPOSITION get_insert_pos(struct command_queue *queue, int dev, int task, unsigned int lba, int *ttl);

/* atac_stddev.c */
void process_query_interface(struct stdservice_query_interface *query_cmd, int task);
int process_stddev(struct stddev_cmd *stddev_msg, int sender_id, struct stddev_res *res);
int check_ownership(int process_id, int logic_device);
int check_ownershipex(int task, int logic_device, int exclusive);

/* ioctrl.c */
int process_ioctrl(struct stddev_ioctl_cmd *cmd, int task, struct stddev_ioctrl_res *res);
int ioctrl_finddev(struct atac_ioctl_finddev *cmd, int task, struct stddev_ioctrl_res *res);
int ioctrl_removeldev(struct atac_ioctl_removeldev *cmd, int task, struct stddev_ioctrl_res *res);
int ioctrl_createldev(struct atac_ioctl_createldev *cmd, int task, struct stddev_ioctrl_res *res);
int ioctrl_enumdevs(struct atac_ioctl_enumdevs *cmd, int task, struct stddev_ioctrl_res *res);

/* Variables */
#define MAX_ADAPTERS 1

extern int ata_adapters_count;
extern struct ata_channel *channelwp_channel;	// used for channel wp initialization
extern struct ata_host_adapter ata_adapters[MAX_ADAPTERS];	// adapters on the system
int check_ownership(int process_id, int logic_device);

#endif



