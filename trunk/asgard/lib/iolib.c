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

#include <lib/iolib.h>
#include <lib/debug.h>

int g_ioport = IOPORT;
int iolasterr = STDFSSERR_OK;

int map_error(int err, int source);

#ifdef IOLIB_MULTITHREADED
static list responses;
static int initialized = 0;
static struct mutex resmutex;
#endif

#define ERRSOURCE_DIR 	0
#define ERRSOURCE_FS  	1
#define ERRSOURCE_PIPES	2

int io_consoleid;

int pipes_task;
int fs_default;

struct SFILE stdin;
struct SFILE stdout;
struct SFILE stderr;

char *ttyname[8] = {"/dev/tty0",
			"/dev/tty1",
			"/dev/tty2",
			"/dev/tty3",
			"/dev/tty4",
			"/dev/tty5",
			"/dev/tty6",
			"/dev/tty7"};
char *pipes_srv_name = "fs/pipes";

void set_ioports(int ioport)
{
	g_ioport = ioport;
}

void initio()
{	
#ifdef IOLIB_MULTITHREADED
	if(!initialized)
	{
		init_mutex(&resmutex);
		init(&responses);
		initialized = 1;
	}
#endif
	fs_default = -1;
	pipes_task = -1;

	// create default FILES for std
	stdin.eof = 0;
	stdin.type = IOLIB_TYPE_FILE;
	stdin.opened = 0;
	stdin.last_error = STDFSSERR_OK;
	stdin.file_id = -1;
	stdin.fs_serviceid = -1;
			
	do
	{		
		resolve_pipes(&stdin);
	}while(stdin.fs_serviceid == -1);

	stdout.eof = 0;
	stdout.type = IOLIB_TYPE_FILE;
	stdout.opened = 0;
	stdout.last_error = STDFSSERR_OK;
	stdout.file_id = -1;
	stdout.fs_serviceid = pipes_task;

	stderr.eof = 0;
	stderr.type = IOLIB_TYPE_FILE;
	stderr.opened = 0;
	stderr.last_error = STDFSSERR_OK;
	stderr.file_id = -1;
	stderr.fs_serviceid = pipes_task;
}

char *ctermid(char *str)
{
	if(io_consoleid == -1) return NULL;	// process has no console

	if(str == NULL)
	{
		return ttyname[io_consoleid];
	}

	istrcopy(ttyname[io_consoleid], str, 0);

	return str;
}

// this function must be called after initio() and before any other commands. File passed must be a PIPE.
void map_std(FILE *stream, FILE *newstream)
{
	if(stream == &stdin)
	{
		fclose(&stdin);
		stdin = *newstream;
		return;
	}
	if(stream == &stdout)
	{
		if(!(stdout.file_id == stderr.file_id && stdout.fs_serviceid == stderr.fs_serviceid))
			fclose(&stdout);
		stdout = *newstream;
		return;
	}
	if(stream == &stderr)
	{
		if(!(stdout.file_id == stderr.file_id && stdout.fs_serviceid == stderr.fs_serviceid))
			fclose(&stderr);
		stderr = *newstream;
		return;
	}	
}

int init_stdstream(FILE *stream)
{
	char *term = NULL;
	FILE *pf = NULL;

	term = ctermid(NULL);

	if(term == NULL || stream == NULL || stream->opened) return 0;

	// stdin will be initialized to the console
	if(stream == &stdin)
	{
		pf = fopen(term, "r");
		if(pf == NULL) return 0;
		stdin = *pf;
		free(pf);
		return 1;
	}
	// initialize to console
	if(stream == &stdout)
	{
		if(stderr.opened && stderr.type == IOLIB_TYPE_FILE)
		{
			stdout = stderr;
		}
		else
		{
			pf = fopen(term, "w");
			if(pf == NULL) return 0;
			stdout = *pf;
			free(pf);
		}
		return 1;
	}
	if(stream == &stderr)
	{
		if(stdout.opened && stdout.type == IOLIB_TYPE_FILE)
		{
			stderr = stdout;
		}
		else
		{
			pf = fopen(term, "w");
			if(pf == NULL) return 0;
			stderr = *pf;
			free(pf);
		}
		return 1;
	}
	return 0;
}

FILE *fopen(char *filename,char *mode)
{
	// get filesystem for the file 
	struct stdfss_open open_msg;
	struct stdfss_open_res open_res;
	FILE *file = (FILE *)malloc(sizeof(FILE));
	int intmode = 0, flags = 0;
	
	file->eof = 0;
	file->type = IOLIB_TYPE_FILE;
	file->opened = 1;
	file->last_error = STDFSSERR_OK;
	file->fs_port = STDFSS_PORT;

	// resolve default fs service //
	file->fs_serviceid = resolve_fs();

	if(file->fs_serviceid == -1)
	{
		free(file);
		return NULL;
	}
	//////////////////////

	// calculate opening mode
	if(streq(mode, "w") || streq(mode, "wb") || streq(mode, "wt"))
	{
		intmode |= STDFSS_FILEMODE_WRITE;
	}
	else if(streq(mode, "r") || streq(mode, "rb") || streq(mode, "rt"))
	{
		intmode |= STDFSS_FILEMODE_READ;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "a")  || streq(mode, "ab") || streq(mode, "at"))
	{
		intmode |= STDFSS_FILEMODE_APPEND;
	}
	else if(streq(mode, "r+") || streq(mode, "rb+") || streq(mode, "rt+") || streq(mode, "r+b") || streq(mode, "r+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_WRITE;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "w+") || streq(mode, "wb+") || streq(mode, "wt+") || streq(mode, "w+b") || streq(mode, "w+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_WRITE;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else if(streq(mode, "a+") || streq(mode, "ab+") || streq(mode, "at+") || streq(mode, "a+b") || streq(mode, "a+t"))
	{
		intmode |= STDFSS_FILEMODE_READ | STDFSS_FILEMODE_APPEND;
		flags |= STDFSS_FILEMODE_MUSTEXIST;
	}
	else
	{
		iolasterr = IOLIBERR_INVALID_MODE;
		return NULL;	
	}
	
	open_msg.command = STDFSS_OPEN;
	open_msg.file_path = share_mem(file->fs_serviceid, filename, len(filename)+1, READ_PERM);
	open_msg.open_mode = intmode;
	open_msg.ret_port = g_ioport;
	open_msg.flags = flags;
#ifdef BUFFERED
	open_msg.flags  |= STDFSS_OPEN_WRITEBUFFERED;
#endif

	send_fs(file->fs_serviceid, (int*)&open_msg, (int*)&open_res, file->fs_port);

	claim_mem(open_msg.file_path);

	if(open_res.ret != STDFSSERR_OK)
	{
		iolasterr = map_error(open_res.ret, ERRSOURCE_FS);
		free(file);
		return NULL;
	}

	file->file_id = open_res.file_id;

	return file;
	
}

FILE *open_spipe(int wtask, int rtask)
{
	// get filesystem for the file 
	struct pipes_openshared open_msg;
	struct pipes_open_res open_res;
	FILE *file = (FILE *)malloc(sizeof(FILE));
	int intmode = 0, flags = 0;
	
	file->eof = 0;
	file->type = IOLIB_TYPE_PIPE;
	file->opened = 1;
	file->last_error = IOLIBERR_OK;

	// resolve pipes service //
	resolve_pipes(file);

	if(file->fs_serviceid == -1)
	{
		free(file);
		return NULL;
	}
	//////////////////////

	open_msg.command = PIPES_OPENSHARED;
	open_msg.ret_port = g_ioport;
	open_msg.task1 = wtask;
	open_msg.task2 = rtask;

	send_fs(file->fs_serviceid, (int*)&open_msg, (int*)&open_res, file->fs_port);

	if(open_res.ret != PIPESERR_OK)
	{
		iolasterr = map_error(open_res.ret, ERRSOURCE_PIPES);
		free(file);
		return NULL;
	}

	file->file_id = open_res.pipeid;

	return file;
}

FILE *open_fpipe(int task, char *filename, int mode)
{
	// get filesystem for the file 
	struct pipes_openfile open_msg;
	struct pipes_open_res open_res;
	FILE *file = (FILE *)malloc(sizeof(FILE));
	int intmode = 0, flags = 0;
	
	file->eof = 0;
	file->type = IOLIB_TYPE_PIPE;
	file->opened = 1;
	file->last_error = IOLIBERR_OK;
	
	// resolve pipes service //
	resolve_pipes(file);

	if(file->fs_serviceid == -1)
	{
		free(file);
		return NULL;
	}

	//////////////////////

	// calculate opening mode
	if(mode & IOLIB_FPIPEMODE_WRITE)
		open_msg.open_mode[0] = 'w';
	else if(mode & IOLIB_FPIPEMODE_APPEND)
		open_msg.open_mode[0] = 'a';
	else
		open_msg.open_mode[0] = 'r';
	open_msg.open_mode[1] = '\0';
	
	open_msg.command = PIPES_OPENFILE;
	open_msg.path_smo = share_mem(file->fs_serviceid, filename, len(filename)+1, READ_PERM);
	open_msg.ret_port = g_ioport;
	open_msg.task = task;

	send_fs(file->fs_serviceid, (int*)&open_msg, (int*)&open_res, file->fs_port);

	claim_mem(open_msg.path_smo);

	if(open_res.ret != PIPESERR_OK)
	{
		iolasterr = map_error(open_res.ret, ERRSOURCE_PIPES);
		free(file);
		return NULL;
	}

	file->file_id = open_res.pipeid;

	return file;
}

int fclose(FILE *stream)
{
	struct stdfss_close close_msg;
	struct stdfss_res res;

	if(stream == NULL || !stream->opened)
	{
		if(stream == &stdin || stream == &stdout || stream == &stderr) return 0; // return ok
		return 1;
	}

	// close the file or pipe
	// NOTE: I'm abusing pipes.h commands are just like stdfss.h except opens. [FIXME]
	close_msg.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_CLOSE : PIPES_CLOSE;
	close_msg.file_id = stream->file_id;
	close_msg.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&close_msg, (int*)&res, stream->fs_port);

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	stream->opened = 0;

	if(stream != &stdin && stream != &stdout & stream != &stderr) free(stream);
	
	return 0;
}; // ret 0 = ok

FILEINFO *finfo(char *filename)
{
	struct stdfss_fileinfo info_cmd;
	struct stdfss_res res;
	int service;
	FILEINFO *finf = (FILEINFO *)malloc(sizeof(FILEINFO));

	// resolve default fs service //
	
	service = resolve_fs();

	if(service == -1)
	{
		free(finf);
		return NULL;
	}

	info_cmd.command = STDFSS_FILEINFO;
	info_cmd.file_info_id = FILEINFO_ID;	
	info_cmd.path_smo = share_mem(service, filename, len(filename)+1, READ_PERM);
	info_cmd.file_info_smo = share_mem(service, finf, sizeof(FILEINFO), READ_PERM | WRITE_PERM);
	info_cmd.ret_port = g_ioport;

	send_fs(service, (int*)&info_cmd, (int*)&res, STDFSS_PORT);

	claim_mem(info_cmd.path_smo);
	claim_mem(info_cmd.file_info_smo);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		
		free(finf);
		return NULL;
	}

	return finf;
}
size_t fwrite(void *buffer, size_t size, size_t count, FILE *stream)
{
	int buffer_size = size * count;
	struct stdfss_write write_cmd;
	struct stdfss_res res;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return 0;
	}

	write_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_WRITE : PIPES_WRITE;
	write_cmd.file_id = stream->file_id;
	write_cmd.count = buffer_size;
	write_cmd.smo = share_mem(stream->fs_serviceid, buffer, buffer_size, READ_PERM);
	write_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&write_cmd, (int *)&res, stream->fs_port);

	claim_mem(write_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);
	
	if(res.ret != STDFSSERR_OK)
	{
		
		return res.param1;
	}

	return res.param1;
}
size_t fread(void *buffer, size_t size, size_t count,FILE *stream)
{
	int buffer_size = size * count;
	struct stdfss_read read_cmd;
	struct stdfss_res res;

	if(stream->eof) return 0;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return 0;
	}

	// read
	read_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_READ : PIPES_READ;
	read_cmd.file_id = stream->file_id;
	read_cmd.count = buffer_size;
	read_cmd.smo = share_mem(stream->fs_serviceid, buffer, buffer_size, WRITE_PERM);
	read_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&read_cmd, (int*)&res, stream->fs_port);

	claim_mem(read_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return res.param1;
	}

	return res.param1;
}

char *fgets(char *str, int len, FILE *stream)
{
	struct stdfss_gets gets_cmd;
	struct stdfss_res res;

	if(stream->eof) return NULL;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return NULL;
	}

	gets_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_GETS : PIPES_GETS;
	gets_cmd.file_id = stream->file_id;
	gets_cmd.count = len;
	gets_cmd.smo = share_mem(stream->fs_serviceid, str, len, WRITE_PERM);
	gets_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&gets_cmd, (int*)&res, stream->fs_port);

	claim_mem(gets_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return NULL;
	}

	return str;
}
int fgetc(FILE *stream)
{
	struct stdfss_getc getc_cmd;
	struct stdfss_res res;

	if(stream->eof) return EOF;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return EOF;
	}

	getc_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_GETC : PIPES_GETC;
	getc_cmd.file_id = stream->file_id;
	getc_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&getc_cmd, (int*)&res, stream->fs_port);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return EOF;
	}

	return ((struct stdfss_getc_res*)&res)->c;
}
int fputc(int c, FILE *stream)
{
	struct stdfss_putc putc_cmd;
	struct stdfss_res res;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return EOF;
	}

	putc_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_PUTC : PIPES_PUTC;
	putc_cmd.file_id = stream->file_id;
	putc_cmd.c = c;
	putc_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&putc_cmd, (int *)&res, stream->fs_port);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);
	
	if(res.ret != STDFSSERR_OK)
	{	
		return EOF;
	}

	return c;
}
int fputs(char *str, FILE *stream)
{
	struct stdfss_puts puts_cmd;
	struct stdfss_res res;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return EOF;
	}

	puts_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_PUTS : PIPES_PUTS;
	puts_cmd.file_id = stream->file_id;
	puts_cmd.count = len(str) + 1;
	puts_cmd.smo = share_mem(stream->fs_serviceid, str, puts_cmd.count, READ_PERM);
	puts_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&puts_cmd, (int *)&res, stream->fs_port);

	claim_mem(puts_cmd.smo);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);
	
	if(res.ret != STDFSSERR_OK)
	{
		
		return EOF;
	}

	return 1;
}

int rm(char *filename)
{
	struct stdfss_delete delete_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	// make dir
	delete_cmd.command = STDFSS_DELETE;
	delete_cmd.path_smo = share_mem(service, filename, len(filename)+1, READ_PERM);
	delete_cmd.ret_port = g_ioport;

	send_fs(service, (int*)&delete_cmd, (int*)&res, STDFSS_PORT);

	claim_mem(delete_cmd.path_smo);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

int mkdir(char *dirpath)
{
	struct stdfss_mkdir mkdir_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	// make dir
	mkdir_cmd.command = STDFSS_MKDIR;
	mkdir_cmd.dir_path = share_mem(service, dirpath, len(dirpath)+1, READ_PERM);
	mkdir_cmd.ret_port = g_ioport;	

	send_fs(service, (int*)&mkdir_cmd, (int*)&res, STDFSS_PORT);

	claim_mem(mkdir_cmd.dir_path);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}
int ioerror(){return iolasterr;}
int ferror(FILE *stream){
	return stream->last_error;
}
int mkdevice(char *path, char *dev_service_name, unsigned int logic_deviceid)
{
	struct stdfss_mkdevice mkdevice_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	mkdevice_cmd.command = STDFSS_MKDEVICE;
	mkdevice_cmd.logic_deviceid = logic_deviceid;
	mkdevice_cmd.path_smo = share_mem(service, path, len(path)+1, READ_PERM);
	mkdevice_cmd.service_name = share_mem(service, dev_service_name, len(dev_service_name)+1, READ_PERM);
	mkdevice_cmd.ret_port = g_ioport;

	send_fs(service,(int*)&mkdevice_cmd, (int *)&res, STDFSS_PORT);

	claim_mem(mkdevice_cmd.path_smo);
	claim_mem(mkdevice_cmd.service_name);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}
	return 0;

}

int umount(char *path)
{
	struct stdfss_umount umount_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	umount_cmd.command = STDFSS_UMOUNT;
	umount_cmd.mount_path = share_mem(service, path, len(path) + 1, READ_PERM);
	umount_cmd.ret_port = g_ioport;

	send_fs(service, (int*)&umount_cmd, (int*)&res, STDFSS_PORT);

	claim_mem(umount_cmd.mount_path);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}
	return 0;
}

int mount(char *device_file, char *mount_path, int mode)
{
	struct stdfss_mount mount_cmd;
	struct stdfss_res res;
	int service;
	
	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	mount_cmd.command = STDFSS_MOUNT;
	mount_cmd.dev_path = share_mem(service, device_file, len(device_file)+1, READ_PERM);
	mount_cmd.mode = mode;
	mount_cmd.mount_path = share_mem(service, mount_path, len(mount_path) + 1, READ_PERM);
	mount_cmd.ret_port = g_ioport;

	send_fs(service, (int*)&mount_cmd, (int *)&res, STDFSS_PORT);

	claim_mem(mount_cmd.dev_path);
	claim_mem(mount_cmd.mount_path);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);
	
	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

int  fseek (FILE * stream , long offset , int origin )
{
	struct stdfss_seek seek_cmd;
	struct stdfss_res res;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return 0;
	}

	// seek on the file 
	seek_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_SEEK : PIPES_SEEK;
	seek_cmd.file_id = stream->file_id;
	seek_cmd.origin = origin;
	seek_cmd.possition = offset;
	seek_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&seek_cmd, (int*)&res, stream->fs_port);

	stream->eof = res.param2;

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

int exists(char *filename)
{
	return filetype(filename) != -1;
}

int filetype(char *filename)
{
	struct stdfss_exists exists_cmd;
	struct stdfss_res res;
	int service;
	
	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return -1;
	}

	exists_cmd.command = STDFSS_EXISTS;
	exists_cmd.path_smo = share_mem(service, filename, len(filename)+1, READ_PERM);
	exists_cmd.ret_port = g_ioport;

	send_fs(service,(int*)&exists_cmd, (int*)&res, STDFSS_PORT);

	claim_mem(exists_cmd.path_smo);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return -1;
	}

	return ((struct stdfss_exists_res *)&res)->type;
}

int ioctl(FILE *stream, int request, long param)
{
	struct stdfss_ioctl ioctl_cmd;
	struct stdfss_ioctl_res res;

	if(stream == NULL || stream == &stdin || stream == &stdout || stream == &stderr) return 1;

	// close the file 
	ioctl_cmd.command = STDFSS_IOCTL;
	ioctl_cmd.file_id = stream->file_id;
	ioctl_cmd.request = request;
	ioctl_cmd.param = param;
	ioctl_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid,(int*)&ioctl_cmd, (int*)&res, STDFSS_PORT);

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return res.dev_ret;
}

long ftell(FILE *stream)
{
	struct stdfss_tell tell_cmd;
	struct stdfss_tell_res res;

	if(stream == NULL || !stream->opened)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return -1;
	}

	// tell
	tell_cmd.command = (stream->type == IOLIB_TYPE_FILE)? STDFSS_TELL : PIPES_TELL;
	tell_cmd.file_id = stream->file_id;
	tell_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&tell_cmd, (int*)&res, stream->fs_port);

	iolasterr = stream->last_error = map_error(res.ret, (stream->type == IOLIB_TYPE_FILE)? ERRSOURCE_FS : ERRSOURCE_PIPES);

	if(res.ret != STDFSSERR_OK)
	{
		return -1;
	}

	return (long)res.cursor;
}

int fflush(FILE *stream)
{
	struct stdfss_flush flush_cmd;
	struct stdfss_res res;

	if(stream == NULL || stream == &stdin || stream == &stdout || stream == &stderr) return 1;

	// flush
	flush_cmd.command = STDFSS_FLUSH;
	flush_cmd.file_id = stream->file_id;
	flush_cmd.ret_port = g_ioport;

	send_fs(stream->fs_serviceid, (int*)&flush_cmd, (int*)&res, STDFSS_PORT);

	iolasterr = stream->last_error = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}

	return 0;
}

int feof(FILE *stream)
{
	if(stream == NULL)
	{
		// stderr and stdout should be mapped to the console
		if(!init_stdstream(stream))
			return 1;
	}
	return stream->eof;
}

int mklink(char *filename, char *linkname)
{
	struct stdfss_link link_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	link_cmd.command = STDFSS_LINK;
	link_cmd.smo_file = share_mem(service, filename, len(filename)+1, READ_PERM);
	link_cmd.smo_link = share_mem(service, linkname, len(linkname) + 1, READ_PERM);
	link_cmd.ret_port = g_ioport;

	send_fs(service,(int*)&link_cmd, (int *)&res, STDFSS_PORT);

	claim_mem(link_cmd.smo_file);
	claim_mem(link_cmd.smo_link);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}
	return 0;
}
int chattr(char * filename, int atts, int substract)
{
	struct stdfss_changeatt catt_cmd;
	struct stdfss_res res;
	int service;

	// resolve default fs service //
	service = resolve_fs();

	if(service == -1)
	{
		return 1;
	}
	//////////////////////

	catt_cmd.command = STDFSS_CHANGEATT;
	catt_cmd.file_path = share_mem(service, filename, len(filename)+1, READ_PERM);
	catt_cmd.flags = atts;
	catt_cmd.substract = substract;
	catt_cmd.ret_port = g_ioport;

	send_fs(service,(int*)&catt_cmd, (int *)&res, STDFSS_PORT);

	claim_mem(catt_cmd.file_path);

	iolasterr = map_error(res.ret, ERRSOURCE_FS);

	if(res.ret != STDFSSERR_OK)
	{
		return 1;
	}
	return 0;
}

int map_error(int err, int source)
{
	if(source == ERRSOURCE_DIR)	
	{
		// directory error
		return err;
	}
	else if(source == ERRSOURCE_FS)
	{
		if(err == 0) return 0;
		// stdfss error
		return err + STDFSS_MSG_MAP;
	}
	else
	{
		switch(err)
		{
			case PIPESERR_OK: return 0;
			case PIPESERR_PIPECLOSED: return IOLIBERR_PIPECLOSED;
			case PIPESERR_FSERROR: return IOLIBERR_FS_ERROR;
			case PIPESERR_EOF: return IOLIBERR_EOF;
			case PIPESERR_INVALIDOP: return IOLIBERR_INVALID_COMMAND;
			default: return IOLIBERR_GENERICERR;
		}
	}
}

char *mapioerr(int ret)
{
	switch(ret)
	{
		case IOLIBERR_OK:
			return "OK";
		case IOLIBERR_INVALID_MODE:
			return "Invalid Mode";
		case IOLIBERR_DIR_NOTREGISTERED:
			return "Not Registered";
		case IOLIBERR_DEVICE_NOT_MOUNTED:
			return "Device not mounted";
		case IOLIBERR_PATH_ALREADY_MOUNTED:
			return "Pah already mounted";
		case IOLIBERR_DEVICE_ALREADY_MOUNTED:
			return "Device already mounted";
		case IOLIBERR_DEVICE_FILE_NOT_FOUND:
			return "File not found";
		case IOLIBERR_SERVICE_NOT_INITIALIZED:
			return "Service not initialized";
		case IOLIBERR_NOT_ENOUGH_MEM:
			return "Not enough memory";
		case IOLIBERR_COULD_NOT_ACCESS_DEVICE:
			return "Could not access device";
		case IOLIBERR_STDFS_SERVICE_SHUTDOWN:
			return "Service is shutting down";
		case IOLIBERR_SERVICE_ALREADY_INITIALIZED:
			return "Service already initialized";
		case IOLIBERR_INTERFACE_NOT_FOUND:
			return "Interface not found";
		case IOLIBERR_DEVICE_ERROR:
			return "Device Error";
		case IOLIBERR_FORMAT_NOT_SUPPORTED:
			return "Format not supported";
		case IOLIBERR_INVALID_COMMAND:	
			return "Invalid command";
		case IOLIBERR_INVALID_COMMAND_PARAMS:
			return "Invalid command parameters";
		case IOLIBERR_EOF:
			return "End of File reached";
		case IOLIBERR_FATAL:
			return "Fatal error";
		case IOLIBERR_NOT_FOUND:
			return "Not found";
		case IOLIBERR_FILE_INUSE:
			return "File is being used";
		case IOLIBERR_FILE_DOESNOTEXIST:
			return "File does not exists";
		case IOLIBERR_FILE_INVALIDOPERATION:
			return "File Invalid operation";
		case IOLIBERR_FILE_DIRNOTEMPTY:
			return "Directory is not empty";
		case IOLIBERR_FILE_NOTOPEN:
			return "File is not open";
		case IOLIBERR_TOOMANY_FILESOPEN:
			return "Too many files open";
		case IOLIBERR_DEVICE_MOUNTED:
			return "Device is mounted";
		case IOLIBERR_PATH_INUSE:
			return "Patyh is in use";
		case IOLIBERR_BAD_DEVICE_FILE:
			return "Bad device file";
		case IOLIBERR_DEVICE_INUSE:
			return "Device is being used";
		case IOLIBERR_FILE_EXISTS:
			return "File already exists";
		case IOLIBERR_DEVICE_NOTSUPPORTED:
			return "Device type is not supported";
		case IOLIBERR_NOT_DEVICEFILE:
			return "Not a device file";
		case IOLIBERR_NOT_SUPPORTED:
			return "Not supported";
		case IOLIBERR_NO_PRIVILEGES:
			return "No privileges";
		case IOLIBERR_SMO_ERROR:
			return "SMO error";
		case IOLIBERR_FS_ERROR:
			return "File system has invalid data. disk may be corrupted.";
		case IOLIBERR_PIPECLOSED:
			return "Pipe has been closed.";
		case IOLIBERR_GENERICERR:
			return "An error has ocurred.";
		case IOLIBERR_FILENAME_TOOLONG:
			return "File name is too long.";
		case IOLIBERR_INVALID_PATH:
			return "Invalid path.";
		default:
			return "Unknown error";
	}
}

int send_fs(int task, int *msg, int *res, int port)
{
	((struct stdfss_cmd *)msg)->thr_id = get_current_thread();

	send_msg(task, port, msg);

	return get_response(task, res);	
}

int resolve_fsex(char *service_name);

void resolve_pipes(FILE *file)
{	
	if(pipes_task != -1)
	{
		file->fs_serviceid = pipes_task;
	}
	else
	{		
		file->fs_serviceid = resolve_fsex(pipes_srv_name);

		if(file->fs_serviceid == -1) return;
		pipes_task = file->fs_serviceid;
	}

	// [FIXME: we should issue STDSERVICE query interface here]
	file->fs_port = 2;
}

int resolve_fs()
{
	if(fs_default == -1)
	{
		fs_default = resolve_fsex("fs/default");
	}
	return fs_default;
}
int resolve_fsex(char *service_name)
{
	struct directory_resolveid resolve_cmd;
	struct directory_response dir_res;

	// resolve default fs service //
	resolve_cmd.command = DIRECTORY_RESOLVEID;
	resolve_cmd.ret_port = g_ioport;
	resolve_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, len(service_name)+1, READ_PERM);
	resolve_cmd.thr_id = get_current_thread();

	send_msg(DIRECTORY_TASK,DIRECTORY_PORT, &resolve_cmd);

	get_response(DIRECTORY_TASK, (int*)&dir_res);

	claim_mem(resolve_cmd.service_name_smo);

	if(dir_res.ret != DIRECTORYERR_OK)
	{
		iolasterr = map_error(dir_res.ret, ERRSOURCE_DIR);
		return -1;
	}

	return dir_res.ret_value;
}

int get_response(int task, int *res)
{
	int id, dir = (task == DIRECTORY_TASK);
	int currthr = get_current_thread();

#ifdef IOLIB_MULTITHREADED
	struct response *resentry = NULL, *ent = NULL;
	CPOSITION entry = NULL, it = NULL;

	// create an entry on the waiting list
	resentry = (struct response *)malloc(sizeof(struct response));

	resentry->thr = currthr;
	resentry->task = task;
	resentry->received = 0;

	wait_mutex(&resmutex);
	add_head(&responses, resentry);
	entry = get_head_position(&responses);
	leave_mutex(&resmutex);
#endif

	for(;;)
	{
#ifdef IOLIB_MULTITHREADED
		// look if there is a response on the io port
		while (get_msg_count(g_ioport) == 0 && !resentry->received) { reschedule(); }

		if(resentry->received)
		{
			wait_mutex(&resmutex);
			// remove our entry
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
			return 1;
		}
#else
		while (get_msg_count(g_ioport) == 0) { reschedule(); }
#endif
		get_msg(g_ioport, res, &id);

		if(dir && id == task && currthr == ((struct directory_response *)res)->thr_id)
		{
#ifdef IOLIB_MULTITHREADED
			wait_mutex(&resmutex);	
			// remove our entry
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
			
#endif
			return 1;
		}
		else if(!dir && id == task && currthr == ((struct stdfss_res *)res)->thr_id)
		{
#ifdef IOLIB_MULTITHREADED
			// remove our entry
			wait_mutex(&resmutex);
			remove_at(&responses, entry);
			leave_mutex(&resmutex);
			free(resentry);
#endif
			return 1;
		}
#ifdef IOLIB_MULTITHREADED
		else
		{
			wait_mutex(&resmutex);
			// message was not ment for us, see if it belongs to other thread
			it = get_head_position(&responses);

			while(it != NULL)
			{
				ent = (struct response *)get_next(&it);

				if( ent->task == id && !ent->received && ((ent->task == DIRECTORY_TASK && ent->thr == ((struct directory_response *)res)->thr_id) || (ent->task != DIRECTORY_TASK && ent->thr == ((struct stdfss_res *)res)->thr_id)))
				{
					// signal
					ent->received = 1;
					break;
				}
			}
			leave_mutex(&resmutex);
		}
#endif

	}
}	

