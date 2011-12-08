bits 32

%include "../include/gdt-syscalls.h"
			
%macro pass_arguments 1
	mov ecx, %1
%%next:
	sub ecx, 1
	push dword [ebp+4*ecx+8]
	jnz %%next
	
%endmacro
	
global init_task
global create_task
global destroy_task
global get_current_task
		
global create_thread
global destroy_thread
global set_thread_run_perms
global set_thread_run_mode
global run_thread
global run_thread_int
global get_current_thread

global grant_page_mk
global page_in
global page_out
global flush_tlb
global get_page_fault
	
global create_int_handler
global destroy_int_handler
global ret_from_int
global get_last_int
global get_last_int_addr

global open_port
global close_port
global set_port_perm
global set_port_mode
global get_msg
global send_msg
global get_msg_count
global get_msgs
global get_msg_counts
	
global share_mem
global claim_mem
global read_mem
global write_mem
global mem_size
global pass_mem

global push_int
global pop_int
global resume_int

global last_error

global ttrace_begin
global ttrace_end
global ttrace_reg
global ttrace_mem_read
global ttrace_mem_write

global evt_set_listener
global evt_wait
global evt_disable

%ifdef _METRICS_
global get_metrics
%endif

create_task:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call CREATE_TASK : 0x00000000
	pop ebp
	ret
	
init_task:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call INIT_TASK : 0x00000000
	pop ebp
	ret
	
destroy_task:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call DESTROY_TASK : 0x00000000
	pop ebp
	ret

get_current_task:
	push ebp
	mov ebp, esp
	call GET_CURRENT_TASK : 0x00000000
	pop ebp
	ret

create_thread:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call CREATE_THREAD : 0x00000000
	pop ebp
	ret
	
destroy_thread:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call DESTROY_THREAD : 0x00000000
	pop ebp
	ret

set_thread_run_perms:	
	push ebp
	mov ebp, esp
	pass_arguments 2
	call SET_THREAD_RUN_PERMS : 0x00000000
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

run_thread_int:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call RUN_THREAD_INT : 0x00000000
	pop ebp
	ret

get_current_thread:
	push ebp
	mov ebp, esp
	call GET_CURRENT_THREAD : 0x00000000
	pop ebp
	ret

page_in:
	push ebp
	mov ebp, esp
	pass_arguments 5
	call PAGE_IN : 0x0000000
	pop ebp
	ret

page_out:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call PAGE_OUT : 0x00000000
	pop ebp
	ret

flush_tlb:
	push ebp
	mov ebp, esp
	call FLUSH_TLB : 0x00000000
	pop ebp
	ret

get_page_fault:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call GET_PAGE_FAULT : 0x00000000
	pop ebp
	ret
	
create_int_handler:
	push ebp
	mov ebp, esp
	pass_arguments 4
	call CREATE_INT_HANDLER : 0x00000000
	pop ebp
	ret

		
destroy_int_handler:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call DESTROY_INT_HANDLER : 0x00000000
	pop ebp
	ret

ret_from_int:
	push ebp
	mov ebp, esp
	call RET_FROM_INT : 0x00000000
	pop ebp
	ret

get_last_int:
	push ebp
	mov ebp, esp
    pass_arguments 1
	call GET_LAST_INT : 0x00000000
	pop ebp
	ret

get_last_int_addr:
	push ebp
	mov ebp, esp
	call GET_LAST_INT_ADDR : 0x00000000
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
	pass_arguments 3
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
	
pop_int:
	push ebp
	mov ebp, esp
	call POP_INT : 0x00000000
	pop ebp
	ret

push_int:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call POP_INT : 0x00000000
	pop ebp
	ret
	
resume_int:
	push ebp
	mov ebp, esp
	call POP_INT : 0x00000000
	pop ebp
	ret

grant_page_mk:
	push ebp
	mov ebp, esp
	pass_arguments 1
	call GRANT_PAGE_MK : 0x00000000
	pop ebp
	ret
	
last_error:
	push ebp
	mov ebp, esp
	call LAST_ERROR : 0x00000000
	pop ebp
	ret


ttrace_begin:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call TTRACE_BEGIN : 0x00000000
	pop ebp
	ret
        
ttrace_end:
    push ebp
	mov ebp, esp
	pass_arguments 2
	call TTRACE_END : 0x00000000
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

evt_set_listener:
	push ebp
	mov ebp, esp
	pass_arguments 3
	call EVT_SET_LISTENER : 0x00000000
	pop ebp
	ret	

evt_wait:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call EVT_WAIT : 0x00000000
	pop ebp
	ret

evt_disable:
	push ebp
	mov ebp, esp
	pass_arguments 2
	call EVT_DISABLE : 0x00000000
	pop ebp
	ret
%ifdef _METRICS_
get_metrics:
	push ebp
	mov ebp, esp
	call GET_METRICS : 0x00000000
	pop ebp
	ret
%endif

