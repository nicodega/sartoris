/*
*	Shell Service.
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
#include <lib/iolib.h>
#include <lib/const.h>
#include <lib/structures/lptree.h>
#include <lib/structures/string.h>
#include <lib/malloc.h>
#include <lib/structures/stdlibsim.h>
#include <lib/scheduler.h>
#include <lib/debug.h>
#include <sartoris/syscall.h>
#include <os/pman_task.h>
#include <ints.h>
#include <lib/structures/list.h>
#include <lib/format.h>
#include <lib/sprintf.h>
#include <services/stdconsole/consoleioctrl.h>
#include <services/stds/stdchardev.h>
#include <services/stds/stddev.h>
#include <services/shell/shell.h>
#include <proc/stdprocess.h>
#include "services/pmanager/services.h"


#define MAX_INTERNAL_PARAMS     20
#define MAX_COMMAND_LINE_LEN    256
#define MAX_COMMAND_LEN         64
#define NUM_TERMS               8
#define BUFFER_SIZE             256
#define MACHINE_NAME            "pistacho"

#define CONS_TASK console_task

extern int console_task;

// NOTE: port 1 is used by the shell protocol for env vars
#define CSL_SCAN_ACK_PORT	2
#define CSL_ACK_PORT		3
#define CSL_SIGNAL_PORT		4
#define CSL_SEEK_PORT		5
#define CSL_REQUEST_PORT	6

#define PM_TASK_ACK_PORT 	7
#define PM_THREAD_ACK_PORT 	8

#define SHELL_INITRET_PORT	9
#define IOLIB_PORT			10
#define DIRLIB_PORT			11

#define INITOFS_PORT 		12

#define INIT_PORT			13

/* Console ownership defines */
#define SHELL_CSLMODE_SHELL	1	// console is being used by the shell service
#define SHELL_CSLMODE_PROC	2	// console has been granted to a process

struct csl_ownership{
	int mode;
	int process;	
	int batched_index;
};

/* Structure for telling which processes are running on from a given console */
struct console_proc_info
{
	int id;			// an internal shell task id (tasks can and will be reused by pman when possible)
	int task;
	int console;
	int running_term;
	int param_smo;
	int map_smo;
	struct map_params maps;
	int stderrout;
	FILE *stdout;
	FILE *stdin;
	FILE *stderr;
	int piped_to_task;	// if piped this will have the task id
	int flags;	
	char *cmd_name;
};

#define PINF_FLAG_NONE         0
#define PINF_FLAG_STDIN_PIPED  1
#define PINF_FLAG_INITIALIZED  2

/* environment variables */

struct env_var_value
{
	char *name;
	char *value;
};

#define ENV_EMPTY_VALUE     (char*)0xFFFFFFFF

/* Functions */
void process_env_msg();
char *shell_get_string(int smo);
int run_internal(int term, char* cmd_line);
int run_command(int term, char* cmd);
int run(int term, char *cmd);
int change_dir(int term, char* dir);
int build_dir(char *dir, char *current_dir, int term);
char *build_path(char *dir, int term);
void term_prepare_input(int term, char echo);

void term_print(int term, char* str);
void term_color_print(int term, char* str, int color);
void term_color_prints(int term, char* str, int color, int size, int buff);
void term_clean(int term);
void show_prompt(int term);

void init_consoles();
void init_environment();
int set_global_env(char *var_name, char *value);
int set_env(char *var_name, char *value, int term);
char *get_env(char *var_name, int term);
char *get_global_env(char *var_name);
void del_env(char *var_name, int term);
void del_global_env(char *var_name);
int exists_env(char *var_name, int term);

void set_cmd(int term, char **args, int argc);
void list_env(int term);
void init_ofs(int term, char **args, int argc);
void mkdir_cmd(int term, char **args, int argc);
void rm_cmd(int term, char **args, int argc);
void ls(int term, char **args, int argc);
void mkdevice_cmd(int term, char **args, int argc);
void ofsformat(int term, char **args, int argc);
void cat(int term, char **args, int argc);
void write(int term, char **args, int argc);
void mount_cmd(int term, char **args, int argc);
void umount_cmd(int term, char **args, int argc);
void mklink_cmd(int term, char **args, int argc);
void chattr_cmd(int term, char **args, int argc);
void contest(int term, char **args, int argc);
void atactst(int term, char **args, int argc);
void dyntst(int term, char **args, int argc);
void jobs(int term, char **args, int argc);
void kill(int term, char **args, int argc);

int get_console(int term);
int free_console(int term);

struct console_proc_info *get_console_process_info(int console);
struct console_proc_info *get_process_info(int task);
struct console_proc_info *process_ack(int task);
void task_finished(int process, int ret_code);
void show_exception(int term, int code, struct console_proc_info *pinf);

struct console_proc_info *execute(int running_term, int console, char *command_path, char *cmd_name, char *params, int pipe, int pipeis_stdin);
int finish_execute(struct console_proc_info *p, FILE *pipe, int pipeis_stdin);
int destroy_tsk(int task);

#define MIN(a,b) ((a > b)? b : a)
#define MAX(a,b) ((a > b)? a : b)

/* variables */
extern list vars[NUM_TERMS + 1];
extern char csl_dir[NUM_TERMS][256];
extern char *tty[NUM_TERMS];
char csl_cmd[NUM_TERMS][MAX_COMMAND_LINE_LEN];
int csl_smo[NUM_TERMS];

/* This shouldn't be initialized here */
extern int console_stddev_port;
extern int console_stdchardev_port;

// terminal ownership flag
extern struct csl_ownership cslown[NUM_TERMS];

/* Command history */
extern char csl_lastcmd[NUM_TERMS][MAX_COMMAND_LINE_LEN];
extern char csl_lastcmdcnt[NUM_TERMS];

/* Init text */
extern char txt_init[];
extern char *txt_csl[];

extern char str_buffer[BUFFER_SIZE];
extern char out_buffer[BUFFER_SIZE];
extern char in_buffer[BUFFER_SIZE];

extern char machine_name[];
extern char user_name[];

extern int argc[NUM_TERMS];

/* Running processes info */
extern list running; // a list of console_proc_info structs

