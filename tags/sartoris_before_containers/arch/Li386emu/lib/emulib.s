%define GETPID_SYSCALL 20
%define KILL_SYSCALL 37
%define SIGUSR1 0xa

%define CREATE_TASK 0
%define DESTROY_TASK 1
%define CREATE_THREAD 2
%define DESTROY_THREAD 3
%define RUN_THREAD 4
%define SEND_MSG 5
%define GET_MSG 6
%define GET_MSG_COUNT 7
%define SHARE_MEM 8
%define CLAIM_MEM 9
%define READ_MEM 10
%define WRITE_MEM 11
%define PASS_MEM 12	
%define SET_THREAD_RUN_PERMS 13
%define SET_THREAD_RUN_MODE 14
%define SET_PORT_PERMS 15
%define SET_PORT_MODE 16
%define CLOSE_PORT 17
%define OPEN_PORT 18

%define EMU_SCR_PRINT 100			

extern main

global _start
global create_task, destroy_task, create_thread, destroy_thread, run_thread
global send_msg, get_msg, get_msg_count
global share_mem, claim_mem, read_mem, write_mem, pass_mem
global set_thread_run_perms, set_thread_run_mode, set_port_perms, set_port_mode, close_port, open_port
global emu_scr_print

global get_reloc
		
%macro sysc 1
	push dword %1
	jmp do_syscall
%endmacro

section .text
	
_start:
	mov eax, GETPID_SYSCALL
	int 0x80
	call get_reloc_ecx
	mov [ecx + pid], eax
	call main
	
finish:	jmp finish
	ret
	
create_task:
	sysc CREATE_TASK
destroy_task:
	sysc DESTROY_TASK
create_thread:
	sysc CREATE_THREAD
destroy_thread
	sysc DESTROY_THREAD
run_thread:
	sysc RUN_THREAD
send_msg:
	sysc SEND_MSG
get_msg:
	sysc GET_MSG
get_msg_count:
	sysc GET_MSG_COUNT
share_mem:
	sysc SHARE_MEM
claim_mem:
	sysc CLAIM_MEM
read_mem:
	sysc READ_MEM
write_mem:
	sysc WRITE_MEM
pass_mem:
	sysc PASS_MEM
set_thread_run_perms:
	sysc SET_THREAD_RUN_PERMS
set_thread_run_mode:
	sysc SET_THREAD_RUN_MODE
set_port_perms:
	sysc SET_PORT_PERMS
set_port_mode:
	sysc SET_PORT_MODE
close_port:
	sysc CLOSE_PORT
open_port:
	sysc OPEN_PORT
	
emu_scr_print:
	sysc EMU_SCR_PRINT
	
;; do_syscall(syscall_code, param1, param2, ...)	
do_syscall:
	push ebx
	push esi
	push edi
	push ebp
	
	mov esi, [esp + 4 * 4]
	lea edi, [esp + 6 * 4]

	
	
	mov eax, KILL_SYSCALL

	call get_reloc_ecx
	mov ebx, [ecx + pid]
	
	mov ecx, SIGUSR1
	int 0x80
	
	pop ebp
	pop edi
	pop esi
	pop ebx
	pop ecx
	ret

get_reloc:
	call helper
dummy:	pop eax
	sub eax, dummy
	ret
helper:
	jmp dummy
	
get_reloc_ecx:
		call helper_ecx
dummy_ecx:	pop ecx
		sub ecx, dummy_ecx
		ret
helper_ecx:
		jmp dummy_ecx

	
	
section .data
pid:	dd 0	
