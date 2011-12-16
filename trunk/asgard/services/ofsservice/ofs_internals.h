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

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <lib/scheduler.h>

#include "bitmaparray.h"

#include <services/stds/stdservice.h>
#include <services/stds/stdfss.h>
#include <services/stds/stddev.h>
#include <services/stds/stdblockdev.h>
#include <services/stds/stdchardev.h>
#include <services/stds/stdservice.h>
#include <lib/malloc.h>
#include <lib/structures/stdlibsim.h>
#include <lib/structures/lptree.h>
#include <lib/structures/list.h>
#include <lib/critical_section.h>
#include <lib/structures/avltree.h>
#include <lib/const.h>
#include <lib/structures/string.h>
#include <services/directory/directory.h>
#include <services/ofsservice/ofs.h>
#include <os/pman_task.h>

//#define DEBUG

#ifndef OFSSINTH
#define OFSSINTH

/*            Features defines 		  */
#define OFS_PATHCACHE
#define OFS_FILEBUFFER_REUSE
#define OFS_DEVBLOCK_CACHE
/******************************************/

#define OFS_THREAD_STACK_SIZE 1024 * 5

// GENERAL //

#define OFS_MAX_CACHED_DEVICES			20
#define OFS_COMMAND_LIFETIME			5
#define OFS_MAXWORKINGTHREADS			1               // thread mutexes are really difficult to test, hence I'll keep it on 1 until I've tested them
#define OFS_MAXINITIALIZED_IDLE_WORKINGTHREADS	5
#define OFS_WOKINGTHREADS_CLEANUPAMMOUNT	    2
#define OFS_MAXTASK_OPENFILES			        10

// FREE BITMAPS //

#define OFS_NODESMODIFIED_MAX	1    // max modifications allowed before the nodes bitmap is persisted
#define OFS_BLOCKSMODIFIED_MAX	1   // max modifications allowed before the blocks bitmap is persisted

// PORTS //

#define OFS_BLOCKDEV_PORT	2
#define OFS_CHARDEV_PORT	3
#define OFS_STDDEVRES_PORT	4
#define OFS_DIRECTORY_PORT	5
#define OFS_PMAN_PORT		6

// this is a port used internally by the ofs service for signaling
#define OFS_IDLE_PORT		7

// BUFFERS //

#define OFS_FILEBUFFERLOCK_OK		0
#define OFS_FILEBUFFERLOCK_NOBUFFER	1

#define OFS_MAXFILEBUFFER_HITS	30

#define OFS_FILEBUFFERS		20  // file buffers available. This value should never be lower than OFS_MAXWORKINGTHREADS
				
#define OFS_NODEBUFFERS		OFS_MAXWORKINGTHREADS

// If these definitions are changed probably lots of code will have to be changed as well
#define OFS_FILE_BUFFERSIZE	OFS_BLOCK_SIZE // File buffers will have a block of size
#define OFS_DIR_BUFFERSIZE	OFS_BLOCK_SIZE // Directory parsing buffers will have a block of size

// NODES LOCKING //

#define OFS_NODELOCK_EXCLUSIVE		0x1
#define OFS_NODELOCK_BLOCKING		0x2
#define OFS_NODELOCK_DIRECTORY		0x4

#define OFS_LOCKSTATUS_OK		0xFFFFFFFF
#define OFS_LOCKSTATUS_DENY(a)		a
#define OFS_LOCKSTATUS_DENYALL		0xFFFFFFFD

// DEVICES LOCKING //

#define OFS_DEVICEREQUEST_EXCLUSIVE	0x1

// device types (device info structure) //

#define DEVTYPE_BLOCK		1
#define DEVTYPE_CHAR		2
#define DEVTYPE_UNDEFINED	3 	// used when device info has not been fetched yet
#define DEVTYPE_INVALID		-1 

// slot assignation //
#define ROOT_DEFAULT_SLOTS OFS_MAXWORKINGTHREADS
#define NONROOT_DEFAULT_SLOTS OFS_MAXWORKINGTHREADS

// start_process responses //
#define OFS_PROCESS_STARTED		0
#define OFS_ENQUEUE			1
#define OFS_PROCESS_NOTSTARTED		-1


#define OFS_THREADSIGNAL_DEVICELOCK	0x1
#define OFS_THREADSIGNAL_NODELOCK	0x2
#define OFS_THREADSIGNAL_MSG		0x3
#define OFS_THREADSIGNAL_BUFFER		0x4
#define OFS_THREADSIGNAL_START		0x5
#define OFS_THREADSIGNAL_DEVICESLOTS	0x6
#define OFS_THREADSIGNAL_DIRECTORYMSG	0x7
#define OFS_THREADSIGNAL_NOSIGNAL	0x8


struct node_lock_waiting
{
	int wpid;
	int nodeid;
	int lock_mode;
	int failed_status;
};

struct smount_info;
struct gc_node;

struct cache_node;

struct path_cache
{
	struct cache_node *root;
	struct sdevice_info *dinf;
	struct mutex mutex;
};


struct node_buffer
{
	unsigned char buffer[OFS_BLOCKDEV_BLOCKSIZE];	// the buffer itself
    	struct mutex buffer_mutex;			// lock for signaling requests
	int first_node_id;				// id of the first node on the block
};

#define OFS_FILEBUFFER_GRACEPERIOD 10 

struct file_buffer
{
	unsigned char buffer[OFS_FILE_BUFFERSIZE];
	int dirty;	// file buffer was changed and MUST be preserved (only happens on Write buffers)
	unsigned int hits;	// how many times was the buffer used since assigned to its task
	unsigned int grace_period;	// this is used in order to avoid a task from keeping a buffer for too long
				// When retrieving buffers, the grace period will be decremented. If it's zero then 
				// this buffer looses priority no matter how many hits it has. and hits will
				// be calculated based on a function f(hits) for comparison. Something like 
				// log(n) would be nice...

	int lockingthreadid;		// id of the thread using the buffer or -1
	unsigned int block_lba;		// block currently on the buffer (if char device then it's -1)

	list opened_files;
	int deviceid;
	int logic_deviceid;
	int waiting_count;
	int waiting_start;
	int processes_waiting_lock[OFS_MAXWORKINGTHREADS];
	int fill_failed;	// this field indicates, although the buffer is present, data is not ok.
	int invalidated;	// used by devblock cache
	struct mutex invalidated_mutex;
};

struct dir_buffer
{
	unsigned char buffer[OFS_DIR_BUFFERSIZE];
};


struct working_thread
{
	int threadid;			 // -1 if thread was never created
	int taskid;
	int deviceid;			 // device on wich the thread is working
	int logic_deviceid;		 // logic device on wich it's working
	struct stdfss_cmd command;	 // command being procesed on the working thread for the task
	int buffer_wait;		// a bit flag. 0 means last command issued was not a buffer
							// operation, 1 means a buffer operation was performed. (used for messages 
							// indentification)
	int locked_device;
	int mounting;

	struct mutex waiting_for_signal_mutex;
	int waiting_for_signal;		// set to 0 when execution can continue, 1 otherwise
	int signal_msg[4];			// message on signal
	int expected_signal_type;	// type of signal which is blocking the working process.
	int signal_type;			// signal type received.
	int signal_senderid;		// service from whom the signal is espected (-1 local)
	int active;					// 0 indicates thread is waiting for processing
								// 1 thread is processing a command
	int initialized;

	struct dir_buffer directory_buffer;	// buffer used for directory parsing on this thread.
	
	struct gc_node *lastdir_parsed_node;
	unsigned int lastdir_parsed_start;
	unsigned int lastdir_parent_nodeid;
	

	int locked_node;	 			// node locked for this thread
	int locked_dirnode;	 			// dir node locked for this thread
	int locked_node_exclusive;		// 0 or 1
	int locked_dirnode_exclusive;	// 0 or 1
	unsigned int node_lock_signal_response;	// when signaled by a nodelock available this will hold
						// a 0xFFFFFFFF if it can be locked or a non zero value 
						// indicating file was ocupied on something.
						// This number will be the id of the dir node
						// the file was parsed from.

	/* device locking:
		This is used as a lock between mount and open commands, because
		different device files could contain the same device info.
		Device locks are all exclusive and blocking.
		This is also used for device signals.
	*/
	int locked_deviceid;
	int locked_logic_deviceid;
	
	/*
		One process will always be waiting for 0 or 1 devices.
		The number on the array indicates the waiting order for processes.
		So the first queued process will have a 1, the second a 2 and so on.
	*/
	int processes_waiting_lockeddevice[OFS_MAXWORKINGTHREADS]; // if processes_waiting_lockeddevice[wpid] > 0 the process is waiting for the device, 0 otherwise
	
	// id of the directory service last sent message
	// used for signaling purposes
	int directory_service_msg;
};

// Device processing info structure //
// If a device is executing a command or has a command waiting this structure will
// hold information on it

struct sblocked_command
{
	struct stdfss_cmd command;	// the pending command
	int task;					// id of the process for whose this command is pending
};

struct sdevice_info
{
	list waiting;			// holds stdfss messages waiting to be processed on the device
	AvlTree procesing;		// tree of commands being processed indexed by task id (only one by task at a given time)
	struct mutex processing_mutex;	// this mutex will be used for checking waiting and procesing lists
	
	int dev_type;			
	int hits;			// used for device info caching
	struct smount_info *mount_info; // if device is not opened as a file this'll hold it's mounting info
	int protocol_port;		// port on wich the interface is implemented
	int stddev_port;		// port on wich the stddev interface is supported.

	int device_nodeid;		// id of the first node for the device file this device was opened from
	int open_count;			// how many times it's opened

	
	struct node_buffer *node_buffers[OFS_NODEBUFFERS]; // node buffers will be held here for each device
	unsigned int block_size;		// used for devices opened as files
	unsigned long max_lba;
	unsigned int ofst_lba;

	int blocked;					// used for char devs response waiting
	struct sblocked_command blocked_command;	// holds info for the blocked command
	struct mutex blocked_mutex;		// mutex used for modifying blocked field

	AvlTree buffers;		// this will hold buffers by lba address assigned on this device for opened files

	AvlTree nodes;			// this will hold nodes by nodeid used by opened files
	struct mutex nodes_mutex;

	int umount;				// 1 if umount operation is being performed, 0 otherwise.
	struct mutex umount_mutex;

	struct path_cache pcache;
};

// indicates a device has a pending job ready to
// be executed concurrently with whatever is executing
struct swaiting_command
{
	struct stdfss_cmd command;  // the pending command
	int sender_id;              // id of the process for whose this command is pending
	int life_time;
};

// idle devices struct //
struct sidle_device
{
	int deviceid;
	int logic_deviceid;
};


// info for mounted OFS block devices //
struct smount_info
{
		struct OFST ofst;		// ofs table info... (probably not the complete ofst)
		unsigned int group_size;	// pre calculated group size	
		struct GH *group_headers;
		struct sdevice_info *dinf;	// device info for the device mounted
		int deviceid;
		int logic_deviceid;
		int mode;			// mount mode used as described in "MOUNT FLAGS"

		bitmap *group_m_free_blocks;
		bitmap *group_m_free_nodes;
		int nodes_modified;
		int blocks_modified;
		struct mutex free_space_mutex;	// mutex used for obtaining free nodes / blocks
										// from bitmaps
};

// task info //
// structure kept on tasks AVL //
struct stask_info
{
	AvlTree open_files;
};

// task file info //
// structure kept on open_files of stask_info
struct stask_file_info
{
	int deviceid;
	int logic_deviceid;
	struct sdevice_info *dinf;	// device on wich the file is located or the device opened
	int mode;					// STDFSS_OPENMODE_XXX indicates how the file was open for a given task
	int buffered;				// TRUE if buffered operation can be performed

	struct gc_node *file_base_node;			// if it's a device opened as a file, this will hold the node of the device file,
											// if its not it'll have the base node of the file

	int file_modified;				
	
	/* read/write cursor and cache */
	struct file_buffer *assigned_buffer;	// buffer used for reading operations
	unsigned long long cursor_possition;
	struct gc_node *current_node;
	unsigned int current_block;				// current node block on buffer, or current block 
	/*used only for devices */
	unsigned int dev_writecount;			// this'll hold bytes written on the buffer for char and block devs buffered
	unsigned int block_offset;
	
	int taskid;								// id of the owner task
	int fileid;

    struct stask_info *takeover;            // if the file has been taken over by a task, this will contain a stask_info structure pointer.
};

#define TAKEOVER_CLOSING (struct stask_info*)0xFFFFFFFF        // this must be an invalid task pointer!

// structures defined for implementation purposes //
struct stdfss_fileid
{
	unsigned char command;   /* Command to execute */
	unsigned short thr_id;	
	unsigned char padding0;
	unsigned short file_id;		// file ID
	unsigned short padding1;	
	unsigned int padding2;			
	unsigned int padding3;
} PACKED_ATT;

// template for device response message //
struct gc_node
{
	struct node n;			// the node
	unsigned int nodeid;	// id of the node 
	int ref;				// references to this node
};

// typedefs //
typedef struct smount_info mount_info;
typedef struct swaiting_command waiting_command;
typedef struct sdevice_info device_info;
typedef struct sidle_device idle_device;
typedef struct stask_info task_info;
typedef struct stask_file_info task_file_info;
typedef struct sdir_entry dir_entry;
typedef struct sdev_file dev_file;

#define MIN(a,b) ((a > b)? b : a)
#define MAX(a,b) ((a > b)? a : b)

// Function definitions //
#ifdef WIN32DEBUGGER
DWORD WINAPI ofs_main(LPVOID lpParameter);
DWORD WINAPI working_process(LPVOID lpParameter);
#else
void ofs_main();
void working_process();
#endif


void close_mutexes();
void close_filebuffer_mutexes();
void close_working_threads_mutexes();
void preserve_file_changes();
void cancel_waiting_commands();
void free_all_structures();
void shutdown();

/* STDFSS commands */
struct stdfss_res *open_file(int wpid, struct working_thread *thread, struct stdfss_open *open_cmd);
struct stdfss_res *close_file(int wpid, struct working_thread *thread, struct stdfss_close *close_cmd);
struct stdfss_res *delete_file(int wpid, struct working_thread *thread, struct stdfss_delete *delete_cmd);
struct stdfss_res *file_info(int wpid, struct working_thread *thread, struct stdfss_fileinfo *fileinfo_cmd);
struct stdfss_res *flush_buffers(int wpid, struct working_thread *thread, struct stdfss_flush *flush_cmd);
struct stdfss_res *seek_file(int wpid, struct working_thread *thread, struct stdfss_seek *seek_cmd);
struct stdfss_res *write_file(int wpid, struct working_thread *thread, struct stdfss_write *write_cmd, int delimited);
struct stdfss_res *read_file(int wpid, struct working_thread *thread, struct stdfss_read *read_cmd, int delimited);
struct stdfss_res *mkdir(int wpid, struct working_thread *thread, struct stdfss_mkdir *mkdir_cmd);
struct stdfss_res *mount_device(int wpid, struct working_thread *thread, struct stdfss_mount *mount_cmd);
struct stdfss_res *mkdevice(int wpid, struct working_thread *thread, struct stdfss_mkdevice *mkdevice_cmd);
struct stdfss_res *umount_device(int wpid, struct working_thread *thread, struct stdfss_umount *umount_cmd);
struct stdfss_res *exists(int wpid, struct working_thread *thread, struct stdfss_exists *exists_cmd);
struct stdfss_res *ioctl(int wpid, struct working_thread *thread, struct stdfss_ioctl *ioctl_cmd);
struct stdfss_res *tell_file(int wpid, struct working_thread *thread, struct stdfss_tell *tell_cmd);
struct stdfss_res *link_file(int wpid, struct working_thread *thread, struct stdfss_link *link_cmd);
struct stdfss_res *change_attributes(int wpid, struct working_thread *thread, struct stdfss_changeatt *changeatt_cmd);

struct stdfss_res *begin_return(int task, struct stdfss_return *cmd, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf);
struct stdfss_res *do_return(int wpid, struct working_thread *thread, struct stdfss_return *cmd);
struct stdfss_res *begin_takeover(int task, struct stdfss_takeover *cmd, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf);
struct stdfss_res *takeover(int wpid, struct working_thread *thread, struct stdfss_takeover *cmd);

/* Character device commands implementation */
struct stdfss_res *chardev_read(int wpid, struct working_thread *thread, struct stdfss_read *read_cmd, int delimited, struct stask_file_info *finf);
struct stdfss_res *chardev_write(int wpid, struct working_thread *thread, struct stdfss_write *write_cmd, int delimited, struct stask_file_info *finf);
int check_blocked_device(struct stdchardev_res *charres, int deviceid);
struct stdfss_res *chardev_seek(int wpid, struct working_thread *thread, struct stdfss_seek *seek_cmd, struct stask_file_info *finf);
struct stdfss_res *chardev_tell(int wpid, struct working_thread *thread, struct stdfss_tell *tell_cmd, struct stask_file_info *finf);

/* Init OFSS command (STDFSS_INIT) */
void init_ofs(struct stdfss_init *init_cmd, struct stdfss_res **ret);

/* Directory parsing function */
int parse_directory(int use_cache, struct gc_node **parsed_node, int lock_mode, int command, struct working_thread *thread, int wpid, struct smount_info *minf, char *path, int path_start, int *flags, unsigned int *out_nodeid, int *out_direntries, int *out_direxists, struct stdfss_res **ret);

/* Device Locking */
int lock_device(int wpid, int deviceid, int logic_deviceid);
int unlock_device(int wpid);

/* File creation */
int create_file(struct gc_node *directory_node, char *file_name, int filename_start, int node_type, int force_preservation, struct smount_info *minf, int wpid, int command, unsigned int *out_nodeid, struct gc_node **out_newnode, int flags, struct stdfss_res **ret);

/* Nodes Management */
int lock_node(int nodeid, int lock_mode, int failed_status, int wpid, int command, struct stdfss_res **ret);
void unlock_node(int wpid, int directory, int status);

int read_node(struct gc_node **fetched_node, unsigned int nodeid, struct smount_info *minf, int wpid, int command, struct stdfss_res **ret);
int write_node(struct gc_node *fetched_node, struct smount_info *minf, int wpid, int command, struct stdfss_res **ret);
struct node_buffer *get_node_buffer(int nodeid, int wpid, struct sdevice_info *dinf);
void free_node_buffer(int nodeid, struct sdevice_info *dinf);
int node_buffer_hash(struct sdevice_info *dinf, int nodeid);

void nfree(struct sdevice_info *dinf, struct gc_node *n);
struct gc_node *nref(struct sdevice_info *dinf, unsigned int nodeid);

/* Devices Management */
int read_device_file(struct gc_node *devicefile_node, struct smount_info *minf, int wpid, int command, int *out_deviceid, int *out_logic_deviceid, struct stdfss_res **ret);
int request_device(int wpid, int deviceid, int logic_deviceid, struct sdevice_info *dinf, int request_mode, int command, struct stdfss_res **ret);
int free_device(int wpid, int deviceid, int logic_deviceid, struct sdevice_info *dinf, int command, struct stdfss_res **ret);
int resolve_name(char *srvname, int *deviceid, int wpid);
int get_device_info(struct stdfss_cmd *cmd, int taskid, int fileid, int *deviceid, int *logic_deviceid, struct sdevice_info **dinf, struct stdfss_res **ret);
void init_logic_device(device_info *logic_device, int devicefile_nodeid, int buffers);

/* Buffers */
int read_buffer(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, struct stdfss_res **ret );
int write_buffer(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, struct stdfss_res **ret );

/* Working processes and signaling */
int locking_send_msg(int totask, int port, void *msg, int wpid);
void get_signal_msg(int *dest, int wpid);
void signal_idle();
void decrement_idle();
int check_idle();
void set_wait_for_signal(int threadid, int signal_type, int senderid);
void wait_for_signal(int threadid);
void signal(int threadid, int *msg, int senderid, int signal_type);
int get_resolution_signal_wp();

/* Bitmaps handling */
unsigned int *get_free_nodes(int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
unsigned int *get_free_blocks(int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void preserve_blocksbitmap_changes(int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void preserve_nodesbitmap_changes(int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void free_node(int preserve, int force_preserve, int node, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void free_block(int preserve, int force_preserve, int block, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void free_bitmap_index(int nodes, int preserve, int force_preserve, int index, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
unsigned int *get_bitmap_freeindexes(int nodes, int count, int force_preserve, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);
void preserve_bitmap_changes(int nodes, int force, struct smount_info *minf, int command, int wpid, struct stdfss_res **ret);

/* File Buffers */
int fb_get(int wpid, int command, struct stask_file_info *finf, int fill, struct stdfss_res **ret);
void fb_finish(int wpid, struct stask_file_info *finf);
int fb_free(int wpid, struct stask_file_info *finf);
int fb_write(int wpid, struct stask_file_info *finf, struct stdfss_res **ret);
void fb_reset(struct file_buffer *fbuffer);
int fb_lock(struct file_buffer *fbuffer, int wpid);

/* Cached Devices */
device_info *get_cache_device_info(int deviceid, int logic_deviceid);
device_info *cache_device_info(int deviceid, int logic_deviceid, struct sdevice_info *dev_inf);
void remove_cached_device();
void remove_cached_deviceE(int deviceid, int logic_deviceid);

/* Helpers */
int check_opened_file(int fileid, int taskid, int base_nodeid, int *open_mode);
int get_file_info(int taskid, int fileid, struct stask_file_info **out_finf, struct stask_info **out_tinf);
unsigned int get_block_group(struct smount_info *minf, int block);
struct stdfss_res *build_response_msg(unsigned int command, unsigned int ret);
int bitmaps_size(int node_count, int block_count);
void mem_copy(unsigned char *source, unsigned char *dest, int count);
char *get_string(int smo);
short get_file_id(struct stask_info * tinf, struct stask_file_info *finf);
void init_file_buffers();
void init_working_threads();
void free_device_struct(device_info *logic_device);
void free_mountinfo_struct(struct smount_info *minf, int wpid);
void clear_dir_buffer(unsigned char *buffer);
int check_concurrent(struct sdevice_info *device, CPOSITION position);
int destroy_working_thread(int id);
int create_working_thread(int id);
int check_waiting_commands();
int get_msg_working_process(int deviceid, int logic_deviceid, int msgid);
int attempt_start(device_info *logic_device, CPOSITION position, int deviceid, int logic_deviceid);
int get_idle_working_thread();
void cleanup_working_threads();
struct stdfss_res *check_path(char *path, int command);

// Global Variables declaration //

extern AvlTree tasks;						// tasks with opened files/devices
extern struct mutex opened_files_mutex;		// used for modifying tasks or opened files
extern list opened_files;					// holds all stask_file_info structures for files

extern list processing_queue;				// devices with pending jobs equeued.
extern struct mutex pending_devices_mutex;
extern lpat_tree mounted;					// info of mounted devices indexed by path
extern struct mutex mounted_mutex;

extern AvlTree cached_devices;			// devices being used indexed by id (same services as mounted). 
										// (each device has a queue on pending requests)
extern int cached_devices_count;
extern struct mutex cached_devices_mutex;

extern char mbuffer[1024 * 1024];		// malloc buffer (1 MB)
extern char working_processes_staks[OFS_MAXWORKINGTHREADS][OFS_THREAD_STACK_SIZE]; // working thread stacks (lets use 5kb per thread... just in case)

extern struct file_buffer file_buffers[OFS_FILEBUFFERS];
extern struct mutex file_buffers_mutex;

extern struct working_thread working_threads[OFS_MAXWORKINGTHREADS];
extern int new_threadid;				// as threads are created, they should read this variable
										// as it'll indicate it's thread id. (threadid mutex should be used
										// to access it
extern int idle_threads;
extern struct mutex idle_threads_mutex;
	
extern int initialized;

extern  list lock_node_waiting;
extern struct mutex node_lock_mutex;
extern struct mutex device_lock_mutex;
	
extern int max_directory_msg;
extern struct mutex max_directory_msg_mutex;

/* Path cache */
#define OFS_MAX_PATHCACHE_SIZE 1024 * 8   // 8 kb

struct cache_node
{
	char *path;
	unsigned int node_id;
	struct gc_node *node;
	unsigned int hits;
	list childs;
	unsigned int size;
	unsigned int flags;
	struct cache_node *parent;
};
void preinit_path_cache(struct path_cache *pc);
void init_path_cache(struct path_cache *pc, struct sdevice_info *dinf, char *mount_path);
struct gc_node *get_cached_node(struct path_cache *pc, char *path, unsigned int *flags, struct gc_node **parent, unsigned int *dir_parentid);
void cache_path_node(struct path_cache *pc, char *path, unsigned int nodeid, unsigned int flags);
struct cache_node *get_closest_match(struct path_cache *pc, char *path, int *out_pos);
void free_cache(struct path_cache *pc);
void cleanup(struct path_cache *pc, struct cache_node *cn);
void invalidate_cache_path(struct path_cache *pc, char *path);

/* Dev Blocks cache */
int bc_read(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, int parser, struct stdfss_res **ret );
int bc_write(char *buffer, unsigned int buffer_size, unsigned int lba, int command, int wpid, struct smount_info *minf, int parser, struct stdfss_res **ret );



#endif

