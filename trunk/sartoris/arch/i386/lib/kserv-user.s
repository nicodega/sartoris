bits 32

;; offsets en la GDT de las syscalls

%include "../include/gdt-syscalls.h"
			
%macro pass_arguments 1
	mov ecx, %1
%%next:
	sub ecx, 1
	push dword [ebp+4*ecx+8]
	jnz %%next
	
%endmacro

global get_current_task

global set_thread_run_mode
global run_thread
global get_current_thread
		
global open_port
global close_port
global set_port_perm
global set_port_mode
global get_msg
global send_msg
global get_msg_count
	
global share_mem
global claim_mem
global read_mem
global write_mem
global mem_size

global last_error
	
get_current_task:
	push ebp
	mov ebp, esp
	call GET_CURRENT_TASK : 0x00000000
	pop ebp
	ret
	
set_thread_run_mode:	
	push ebp
	mov ebp, esp
	pass_arguments 3
	call SET_THREAD_RUN_MODE : 0x00000000
	pop ebp
	ret
	
run_thread:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call RUN_THREAD : 0x00000000
	pop ebp
	ret
	
get_current_thread:
	push ebp
	mov ebp, esp
	call GET_CURRENT_THREAD : 0x00000000
	pop ebp
	ret

open_port:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call OPEN_PORT : 0x00000000
	pop ebp
	ret

close_port:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call CLOSE_PORT : 0x00000000
	pop ebp
	ret

set_port_perm:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call SET_PORT_PERM : 0x00000000
	pop ebp
	ret

set_port_mode:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call SET_PORT_MODE : 0x00000000
	pop ebp
	ret
	
send_msg:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call SEND_MSG : 0x00000000
	pop ebp
	ret

get_msg:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call GET_MSG : 0x00000000
	pop ebp
	ret
	
get_msg_count:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call GET_MSG_COUNT : 0x00000000
	pop ebp
	ret
	
share_mem:
	push ebp
	mov ebp, esp
	pass_arguments 4
	call SHARE_MEM : 0x00000000
	pop ebp
	ret

claim_mem:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call CLAIM_MEM : 0x00000000
	pop ebp
	ret

read_mem:	
	push ebp
	mov ebp, esp
	pass_arguments 4
	call READ_MEM : 0x00000000
	pop ebp
	ret

write_mem:
	push ebp
	mov ebp, esp
	pass_arguments 4
	call WRITE_MEM : 0x00000000
	pop ebp
	ret

pass_mem:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call PASS_MEM :	 0x00000000
	pop ebp
	ret

mem_size:
	push ebp		
	mov ebp, esp
	pass_arguments 1
	call MEM_SIZE : 0x00000000
	pop ebp
	ret
	
last_error:
	push ebp
	mov ebp, esp
	call LAST_ERROR : 0x00000000
	pop ebp
	ret