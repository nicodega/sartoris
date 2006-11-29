/*
*
*	IOLIB 0.1: An implementation of an input/output library using the directory and 
*		   the ofs service implementation.
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

#include <sartoris/syscall.h>
#include <services/stds/stdfss.h>
#include <services/stds/stdservice.h>
#include <lib/structures/string.h>
#include <lib/const.h>
#include <lib/structures/stdlibsim.h>
#include <lib/malloc.h>
#include <lib/critical_section.h>
#include <services/directory/directory.h>
#include <os/pman_task.h>
#include <lib/scheduler.h>
#include <services/pipes/pipes.h>

#ifndef IOLIBH
#define IOLIBH


#define SEEK_SET STDFSS_SEEK_SET
#define SEEK_CUR STDFSS_SEEK_CUR
#define SEEK_END STDFSS_SEEK_END

#define EOF -1

#ifdef IOLIB_MULTITHREADED
#include <lib/structures/list.h>

struct response
{
	int thr;	// waiting thread
	int task;	// task from whom the message must be received
	int received;	// initializero, if message arrives, it'll be set to 1
	int res[4];
};
#endif

#define IOLIB_TYPE_FILE		1
#define IOLIB_TYPE_PIPE		2

struct SFILE{
	int fs_serviceid;
	short file_id;
	int eof;
	int last_error;
	int type;	// whether it's a pipe or a file
	int opened;	// only used for stds
	int fs_port;

	// process side buffering
	// NOTE: this is disabled just because I didn't implement it
	// it really does not seem so difficult to implement
/*
	char *buffer;	
	unsigned int possition;		// possition on buffer
	int length;			// buffer length
	int open_mode;			// file open mode
	unsigned long long file_size;	// this is necesary in order to clean the buffer on seeks
					// on open, a file_info should be issued to get file size
	int buffer_mode;
*/
};

/*Std in/out */
extern struct SFILE stdin;
extern struct SFILE stdout;
extern struct SFILE stderr;

#define IOPORT 5
#define L_ctermid 64

typedef struct SFILE FILE;
typedef struct file_info0 FILEINFO;

extern int iolasterr;
extern int io_consoleid; 	// console number


void map_std(FILE *stream, FILE *newstream); // map an std stream 

FILE *open_spipe(int wtask, int rtask);

#define IOLIB_FPIPEMODE_READ	1
#define IOLIB_FPIPEMODE_WRITE	2
#define IOLIB_FPIPEMODE_APPEND	4

FILE *open_fpipe(int task, char *filename, int mode);

char *ctermid(char *str);
FILE *fopen(char *filename, char *mode);
int fclose(FILE *stream);
FILEINFO *finfo(char *filename);
size_t fwrite(void *buffer, size_t size, size_t count, FILE *stream);
size_t fread(void *buffer, size_t size, size_t count,FILE *stream);
int mkdir(char *dirpath);
int mklink(char *filename, char *linkname);
int chattr(char * filename, int atts, int substract);
int rm(char *filename);
int ferror(FILE *stream);
int ioerror();
int mkdevice(char *path, char *dev_service_name, unsigned int logic_deviceid);
int mount(char *device_file, char *mount_path, int mode);
int umount(char *path);
int fseek (FILE * stream , long offset , int origin );
long ftell(FILE *stream);
int feof(FILE *stream);
int exists(char *filename);
int filetype(char *filename);
int ioctl(FILE *stream, int request, long param);
int fflush(FILE *stream);
char *mapioerr(int ret);

char *fgets(char *str, int len, FILE *);
int fgetc(FILE *);
int fputc(int, FILE *);
int fputs(char *, FILE *);


void set_ioports(int ioport);
void initio();

/* intenal */
int resolve_fs();
void resolve_pipes(FILE *file);
int send_fs(int task, int *msg, int *res, int port);
int get_response(int task, int *res);
int init_stdstream(FILE *stream);

/* defines */

#define STDFSS_MSG_MAP 4

#define IOLIB_ATT_READONLY	STDFSS_ATT_READONLY
#define IOLIB_ATT_ENCRYPTED	STDFSS_ATT_ENCRYPTED
#define IOLIB_ATT_HIDDEN	STDFSS_ATT_HIDDEN

#define IOLIB_FILETYPE_DIRECTORY 	STDFSS_FILETYPE_DIRECTORY
#define IOLIB_FILETYPE_FILE		STDFSS_FILETYPE_FILE 
#define IOLIB_FILETYPE_DEVICE 		STDFSS_FILETYPE_DEVICE 

#define IOLIB_MOUNTMODE_READ 		STDFSS_MOUNTMODE_READ
#define IOLIB_MOUNTMODE_WRITE 		STDFSS_MOUNTMODE_WRITE	
#define IOLIB_MOUNTMODE_DEDICATED 	STDFSS_MOUNTMODE_DEDICATED

/* Error definitions */
#define IOLIBERR_OK						0
#define IOLIBERR_INVALID_MODE  					1
#define IOLIBERR_DIR_NOTREGISTERED				2

/* Pipes */
#define IOLIBERR_PIPECLOSED 					3
#define IOLIBERR_GENERICERR					4	// generic error
/* stdfss */
#define IOLIBERR_DEVICE_NOT_MOUNTED		STDFSSERR_DEVICE_NOT_MOUNTED + STDFSS_MSG_MAP
#define IOLIBERR_PATH_ALREADY_MOUNTED		STDFSSERR_PATH_ALREADY_MOUNTED + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_ALREADY_MOUNTED		STDFSSERR_DEVICE_ALREADY_MOUNTED + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_FILE_NOT_FOUND		STDFSSERR_DEVICE_FILE_NOT_FOUND + STDFSS_MSG_MAP
#define IOLIBERR_SERVICE_NOT_INITIALIZED	STDFSSERR_SERVICE_NOT_INITIALIZED + STDFSS_MSG_MAP
#define IOLIBERR_NOT_ENOUGH_MEM			STDFSSERR_NOT_ENOUGH_MEM + STDFSS_MSG_MAP
#define IOLIBERR_COULD_NOT_ACCESS_DEVICE	STDFSSERR_COULD_NOT_ACCESS_DEVICE + STDFSS_MSG_MAP
#define IOLIBERR_STDFS_SERVICE_SHUTDOWN		STDFSSERR_STDFS_SERVICE_SHUTDOWN + STDFSS_MSG_MAP
#define IOLIBERR_SERVICE_ALREADY_INITIALIZED	STDFSSERR_SERVICE_ALREADY_INITIALIZED + STDFSS_MSG_MAP
#define IOLIBERR_INTERFACE_NOT_FOUND		STDFSSERR_INTERFACE_NOT_FOUND + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_ERROR			STDFSSERR_DEVICE_ERROR + STDFSS_MSG_MAP
#define IOLIBERR_FORMAT_NOT_SUPPORTED		STDFSSERR_FORMAT_NOT_SUPPORTED + STDFSS_MSG_MAP
#define IOLIBERR_INVALID_COMMAND		STDFSSERR_INVALID_COMMAND + STDFSS_MSG_MAP
#define IOLIBERR_INVALID_COMMAND_PARAMS		STDFSSERR_INVALID_COMMAND_PARAMS + STDFSS_MSG_MAP
#define IOLIBERR_EOF				STDFSSERR_EOF + STDFSS_MSG_MAP
#define IOLIBERR_FATAL				STDFSSERR_FATAL + STDFSS_MSG_MAP
#define IOLIBERR_NOT_FOUND			STDFSSERR_NOT_FOUND + STDFSS_MSG_MAP
#define IOLIBERR_FILE_INUSE			STDFSSERR_FILE_INUSE + STDFSS_MSG_MAP
#define IOLIBERR_FILE_DOESNOTEXIST		STDFSSERR_FILE_DOESNOTEXIST + STDFSS_MSG_MAP
#define IOLIBERR_FILE_INVALIDOPERATION		STDFSSERR_FILE_INVALIDOPERATION + STDFSS_MSG_MAP
#define IOLIBERR_FILE_DIRNOTEMPTY		STDFSSERR_FILE_DIRNOTEMPTY + STDFSS_MSG_MAP
#define IOLIBERR_FILE_NOTOPEN			STDFSSERR_FILE_NOTOPEN + STDFSS_MSG_MAP
#define IOLIBERR_TOOMANY_FILESOPEN		STDFSSERR_TOOMANY_FILESOPEN + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_MOUNTED			STDFSSERR_DEVICE_MOUNTED + STDFSS_MSG_MAP
#define IOLIBERR_PATH_INUSE			STDFSSERR_PATH_INUSE + STDFSS_MSG_MAP
#define IOLIBERR_BAD_DEVICE_FILE		STDFSSERR_BAD_DEVICE_FILE + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_INUSE			STDFSSERR_DEVICE_INUSE + STDFSS_MSG_MAP
#define IOLIBERR_FILE_EXISTS			STDFSSERR_FILE_EXISTS + STDFSS_MSG_MAP
#define IOLIBERR_DEVICE_NOTSUPPORTED		STDFSSERR_DEVICE_NOTSUPPORTED + STDFSS_MSG_MAP
#define IOLIBERR_NOT_DEVICEFILE			STDFSSERR_NOT_DEVICEFILE + STDFSS_MSG_MAP
#define IOLIBERR_NOT_SUPPORTED			STDFSSERR_NOT_SUPPORTED + STDFSS_MSG_MAP
#define IOLIBERR_NO_PRIVILEGES			STDFSSERR_NO_PRIVILEGES + STDFSS_MSG_MAP
#define IOLIBERR_SMO_ERROR			STDFSSERR_SMO_ERROR + STDFSS_MSG_MAP
#define IOLIBERR_FS_ERROR			STDFSSERR_FS_ERROR  + STDFSS_MSG_MAP
#define IOLIBERR_FILENAME_TOOLONG		STDFSSERR_FILENAME_TOOLONG  + STDFSS_MSG_MAP
#define IOLIBERR_INVALID_PATH		STDFSSERR_INVALID_PATH + STDFSS_MSG_MAP

#endif

