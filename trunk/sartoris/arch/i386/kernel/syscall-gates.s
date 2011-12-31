bits 32

	;; this is horrible, and will have to be updated.
	;; consider passing arguments using registers the next time.
	
extern create_task
global create_task_c

extern init_task
global init_task_c

extern destroy_task
global destroy_task_c

extern get_current_task
global get_current_task_c
		
extern create_thread
global create_thread_c

extern destroy_thread
global destroy_thread_c
	
extern run_thread_int
global run_thread_int_c

extern run_thread
global run_thread_c

extern set_thread_run_perms
global set_thread_run_perms_c

extern set_thread_run_mode
global set_thread_run_mode_c

extern get_current_thread
global get_current_thread_c

extern page_in
global page_in_c

extern page_out
global page_out_c

extern flush_tlb
global flush_tlb_c

extern get_page_fault
global get_page_fault_c
		
extern create_int_handler
global create_int_handler_c

extern destroy_int_handler
global destroy_int_handler_c

extern ret_from_int
global ret_from_int_c

extern get_last_int
global get_last_int_c

extern get_last_int_addr
global get_last_int_addr_c	

extern open_port
global open_port_c

extern close_port
global close_port_c

extern set_port_perm
global set_port_perm_c

extern set_port_mode
global set_port_mode_c
		
extern get_msg
global get_msg_c

extern send_msg
global send_msg_c

extern get_msg_count
global get_msg_count_c

extern get_msgs
global get_msgs_c

extern get_msg_counts
global get_msg_counts_c

extern share_mem
global share_mem_c

extern claim_mem
global claim_mem_c

extern read_mem
global read_mem_c

extern write_mem
global write_mem_c

extern pass_mem
global pass_mem_c

extern mem_size
global mem_size_c

extern last_error
global last_error_c	

%ifdef _METRICS_
extern get_metrics
global get_metrics_c
%endif	

extern pop_int
global pop_int_c

extern push_int
global push_int_c

extern resume_int
global resume_int_c

extern grant_page_mk
global grant_page_mk_c

extern ttrace_begin
global ttrace_begin_c

extern ttrace_end
global ttrace_end_c

extern ttrace_reg
global ttrace_reg_c

extern ttrace_mem_read
global ttrace_mem_read_c

extern ttrace_mem_write
global ttrace_mem_write_c

extern evt_set_listener
global evt_set_listener_c
extern evt_wait
global evt_wait_c
extern evt_disable
global evt_disable_c

extern curr_state
extern stack_winding_syscall
extern stack_unwind_syscall

;; Intra-privilege level call
;; On the new stack, the processor pushes the segment selector and stack pointer for the calling procedure's stack, an (optional) 
;; set of parameters from the calling procedures stack, and the segment selector and instruction pointer for the calling procedure's 
;; code segment. (A value in the call gate descriptor determines how many parameters to copy to the new stack.) Finally, the processor 
;; branches to the address of the procedure being called within the new code segment.

%macro syscall_def_np 1
    push dword 0
    call stack_winding_syscall
	mov ecx, %1
	jmp do_syscall_no_args
%endmacro

%macro syscall_def 2
    push %1
    call stack_winding_syscall
    ;; stack_winding_syscall will leave how many bytes it used from the stack (not counting out push)
    add eax, 4      ;; count our push
	mov ecx, %2
	mov edx, %1
	call do_syscall
    call stack_unwind_syscall
    pop ecx
	retf (%1*4)
%endmacro

;; tasking
create_task_c:
	syscall_def 2, create_task
	
init_task_c:
	syscall_def 3, init_task

destroy_task_c:
	syscall_def 1, destroy_task

get_current_task_c:
	syscall_def_np get_current_task
	
	;; threading
	
create_thread_c:
	syscall_def 2, create_thread

destroy_thread_c:
	syscall_def 1, destroy_thread

set_thread_run_perms_c:
	syscall_def 2, set_thread_run_perms

set_thread_run_mode_c:
	syscall_def 3, set_thread_run_mode

get_current_thread_c:
	syscall_def_np get_current_thread

	;; paging

page_in_c:
	syscall_def 5, page_in

page_out_c:
	syscall_def 3, page_out

flush_tlb_c:
	syscall_def_np flush_tlb

get_page_fault_c:
	syscall_def 1, get_page_fault
			
grant_page_mk_c:
	syscall_def 1, grant_page_mk
	
	;; interrupt handling

create_int_handler_c:
	syscall_def 4, create_int_handler

destroy_int_handler_c:
	syscall_def 2, destroy_int_handler

	;; I will inline ret_from_int, because it is called
	;; so bloody often.
ret_from_int_c:
    push dword 0
	call stack_winding_syscall
	push ds				; switch to kernel space
	push es
	mov eax, 0x10
	mov ds, eax
	mov es, eax
	mov ecx, ret_from_int
	call ecx			; make call
	
	pop es				; back to userland
	pop ds	
	call stack_unwind_syscall
    add esp, 4          ; for the first push
	retf 0

get_last_int_c:	
	syscall_def 1, get_last_int

get_last_int_addr_c:
	syscall_def_np get_last_int_addr
    		
	;; messaging
	
close_port_c:
	syscall_def 1, close_port
	
open_port_c:
	syscall_def 3, open_port
	
set_port_perm_c:
	syscall_def 2, set_port_perm

set_port_mode_c:
	syscall_def 3, set_port_mode
			
send_msg_c:
	syscall_def 3, send_msg

get_msg_c:
	syscall_def 3, get_msg

get_msg_count_c:
	syscall_def 1, get_msg_count

get_msgs_c:
	syscall_def 4, get_msgs

get_msg_counts_c:
	syscall_def 3, get_msg_counts

	;; memory sharing
	
share_mem_c:
	syscall_def 4, share_mem

claim_mem_c:
	syscall_def 1, claim_mem

read_mem_c:
	syscall_def 4, read_mem

write_mem_c:
	syscall_def 4, write_mem

pass_mem_c:
	syscall_def 2, pass_mem

mem_size_c:
	syscall_def 1, mem_size

run_thread_c:
	syscall_def 1, run_thread

run_thread_int_c:
	syscall_def 3, run_thread
	
;; int stack manipulation
push_int_c:
	syscall_def 1, push_int

pop_int_c:
	syscall_def_np pop_int

resume_int_c:
	syscall_def_np resume_int

;; error

last_error_c:
	syscall_def_np last_error
	

;; tracing

ttrace_begin_c:
    syscall_def 2, ttrace_begin

ttrace_end_c:
    syscall_def 2, ttrace_end

ttrace_reg_c:
    syscall_def 4, ttrace_reg

ttrace_mem_read_c:
    syscall_def 4, ttrace_mem_read

ttrace_mem_write_c:
    syscall_def 4, ttrace_mem_write

evt_set_listener_c:
    syscall_def 3, evt_set_listener

evt_wait_c:
    syscall_def 3, evt_wait

evt_disable_c:
    syscall_def 3, evt_disable

;; metrics
			
%ifdef _METRICS_
get_metrics_c:
	syscall_def 1, get_metrics
%endif	
		
do_syscall:
    ;; eax has how many bytes we must skip on the stack (because of the winding and param count push)
	push ebp
	mov ebp, esp

    push ebx
	push ds				; switch to kernel space
	push es
	mov ebx, 0x10
	mov ds, ebx
	mov es, ebx
    
    add eax, ebp        ; now eax points to the last parameter

	push esi
	mov esi, edx		; save parameter count
	
pass_argument:

	sub edx, 1
	push dword [eax+edx*4+16]	; pass edx arguments (16 = eip, eip far call, cs far call)
	jnz pass_argument
		
	call ecx			; make call
	
	shl esi, 2
	add esp, esi		; free stack space
	
	pop esi
	pop es				; back to userland
	pop ds
    pop ebx
	pop ebp
	ret

do_syscall_no_args:
	push ds				; switch to kernel space
	push es
	mov eax, 0x10
	mov ds, eax
	mov es, eax
	
	call ecx			; make call
	
	pop es				; back to userland
	pop ds	
	call stack_unwind_syscall
    add esp, 4          ; pop params count
	retf 0 

	
