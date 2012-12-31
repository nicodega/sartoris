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
global get_msgs
global get_msg_counts
	
global ret_from_int

global share_mem
global claim_mem
global read_mem
global write_mem
global mem_size

global last_error

global ttrace_reg
global ttrace_mem_read
global ttrace_mem_write
	
get_current_task:
	push ebp
	mov ebp, esp
	call GET_CURRENT_TASK : 0x00000000
	pop ebp
	ret
	
set_thread_run_mode:	
	push ebp
	mov ebp, esp
	call SET_THREAD_RUN_MODE : 0x00000000
	pop ebp
	ret
	
run_thread:
	push ebp
	mov ebp, esp
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
	call OPEN_PORT : 0x00000000
	pop ebp
	ret

close_port:
	push ebp
	mov ebp, esp
	call CLOSE_PORT : 0x00000000
	pop ebp
	ret

set_port_perm:
	push ebp
	mov ebp, esp
	call SET_PORT_PERM : 0x00000000
	pop ebp
	ret

set_port_mode:
	push ebp
	mov ebp, esp
	call SET_PORT_MODE : 0x00000000
	pop ebp
	ret
	
send_msg:
	push ebp
	mov ebp, esp
	call SEND_MSG : 0x00000000
	pop ebp
	ret

get_msg:
	push ebp
	mov ebp, esp
	call GET_MSG : 0x00000000
	pop ebp
	ret
	
get_msg_count:
	push ebp
	mov ebp, esp
	call GET_MSG_COUNT : 0x00000000
	pop ebp
	ret
    
get_msgs:
	push ebp
	mov ebp, esp
	pass_arguments 4
	call GET_MSGS : 0x00000000
	pop ebp
	ret
	
get_msg_counts:
	push ebp
	mov ebp, esp
	call GET_MSG_COUNTS : 0x00000000
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
	call PASS_MEM :	 0x00000000
	pop ebp
	ret

mem_size:
	push ebp		
	mov ebp, esp
	call MEM_SIZE : 0x00000000
	pop ebp
	ret
	
last_error:
	push ebp
	mov ebp, esp
	call LAST_ERROR : 0x00000000
	pop ebp
	ret

ttrace_reg:
    push ebp
	mov ebp, esp
	pass_arguments 4
	call TTRACE_REG : 0x00000000
	pop ebp
	ret

ttrace_mem_read:
    push ebp
	mov ebp, esp
	pass_arguments 4
	call TTRACE_MEM_READ : 0x00000000
	pop ebp
	ret

ttrace_mem_write:
	push ebp
	mov ebp, esp
	pass_arguments 4
	call TTRACE_MEM_WRITE : 0x00000000
	pop ebp
	ret

ret_from_int:
	push ebp
	mov ebp, esp
	call RET_FROM_INT : 0x00000000
	pop ebp
	ret

