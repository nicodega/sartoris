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
	
extern run_thread
global run_thread_c

extern run_thread_int
global run_thread_int_c

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

extern curr_state

;; tasking
create_task_c:
	mov ecx, create_task
	mov edx, 2
	call do_syscall
	retf 8
	
init_task_c:
	mov ecx, init_task
	mov edx, 3
	call do_syscall
	retf 12

destroy_task_c:
	mov ecx, destroy_task
	mov edx, 1
	call do_syscall
	retf 4

get_current_task_c:
	mov ecx, get_current_task
	jmp do_syscall_no_args
	
	;; threading
	
create_thread_c:
	mov ecx, create_thread
	mov edx, 2
	call do_syscall
	retf 8

destroy_thread_c:
	mov ecx, destroy_thread
	mov edx, 1
	call do_syscall
	retf 4

set_thread_run_perms_c:
	mov ecx, set_thread_run_perms
	mov edx, 2
	call do_syscall
	retf 8

set_thread_run_mode_c:
	mov ecx, set_thread_run_mode
	mov edx, 3
	call do_syscall
	retf 12

get_current_thread_c:
	mov ecx, get_current_thread
	jmp do_syscall_no_args

	;; paging

page_in_c:
	mov ecx, page_in
	mov edx, 5
	call do_syscall
	retf 20

page_out_c:
	mov ecx, page_out
	mov edx, 3
	call do_syscall
	retf 12

flush_tlb_c:
	mov ecx, flush_tlb
	call do_syscall_no_args
	retf 0

get_page_fault_c:
	mov ecx, get_page_fault
	mov edx, 1
	call do_syscall
	retf 4
			
grant_page_mk_c:
	mov ecx, grant_page_mk
	mov edx, 1
	call do_syscall
	retf 4
	
	;; interrupt handling

create_int_handler_c:
	mov ecx, create_int_handler
	mov edx, 4
	call do_syscall
	retf 16

destroy_int_handler_c:
	mov ecx, destroy_int_handler
	mov edx, 2
	call do_syscall
	retf 8

	;; I will inline ret_from_int, because it is called
	;; so bloody often.
ret_from_int_c:
	push ds				; switch to kernel space
	push es
	mov eax, 0x10
	mov ds, eax
	mov es, eax
	mov ecx, ret_from_int
	call ecx			; make call
	
	pop es				; back to userland
	pop ds	
	retf 0 

get_last_int_c:	
	mov ecx, get_last_int
	mov edx, 1
	call do_syscall
	retf 4
		
	;; messaging
	
close_port_c:
	mov ecx, close_port
	mov edx, 1
	call do_syscall
	retf 4
	
open_port_c:
	mov ecx, open_port
	mov edx, 3
	call do_syscall
	retf 12
	
set_port_perm_c:
	mov ecx, set_port_perm
	mov edx, 2
	call do_syscall
	retf 8

set_port_mode_c:
	mov ecx, set_port_mode
	mov edx, 3
	call do_syscall
	retf 12
			
send_msg_c:
	mov ecx, send_msg
	mov edx, 3
	call do_syscall
	retf 12

get_msg_c:
	mov ecx, get_msg
	mov edx, 3
	call do_syscall
	retf 12

get_msg_count_c:
	mov ecx, get_msg_count
	mov edx, 1
	call do_syscall
	retf 4

	;; memory sharing
	
share_mem_c:
	mov ecx, share_mem
	mov edx, 4
	call do_syscall
	retf 16

claim_mem_c:
	mov ecx, claim_mem
	mov edx, 1
	call do_syscall
	retf 4

read_mem_c:
	mov ecx, read_mem
	mov edx, 4
	call do_syscall
	retf 16

write_mem_c:
	mov ecx, write_mem
	mov edx, 4
	call do_syscall
	retf 16

pass_mem_c:
	mov ecx, pass_mem
	mov edx, 2
	call do_syscall
	retf 8

mem_size_c:
	mov ecx, mem_size
	mov edx, 1
	call do_syscall
	retf 4

run_thread_c:
	mov ecx, run_thread
	mov edx, 1
	call do_syscall
	retf 4

run_thread_int_c:
	mov ecx, run_thread_int
	mov edx, 3
	call do_syscall
	retf 12
	
;; int stack manipulation
push_int_c:
	mov ecx, push_int
	mov edx, 1
	call do_syscall
	retf 4

pop_int_c:
	mov ecx, pop_int
	jmp do_syscall_no_args
	
resume_int_c:
	mov ecx, resume_int
	jmp do_syscall_no_args

;; error

last_error_c:
	mov ecx, last_error
	jmp do_syscall_no_args
	
;; metrics
			
%ifdef _METRICS_
get_metrics_c:
	mov ecx, get_metrics
	mov edx, 1
	call do_syscall
	retf 4
%endif	
			
do_syscall:
	push ebp
	mov ebp, esp

	push ds				; switch to kernel space
	push es
	mov eax, 0x10
	mov ds, eax
	mov es, eax

	push esi
	mov esi, edx		; save parameter count
	
	
pass_argument:

	sub edx, 1
	push dword [ebp+edx*4+16]	; pass edx arguments
	jnz pass_argument
		
	call ecx			; make call
	
	shl esi, 2
	add esp, esi		; free stack space
	
	pop esi
	
	pop es				; back to userland
	pop ds

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
	retf 0 

	
