
	;; Sartoris i386 interrupt support
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com
	
	;; initially, the kernel will dump registers and die in response to
	;; an exception. the 'exceptions_call_table' is used for that.
	;; however, the kernel handles interrupts and exceptions in a
	;; similar fashion. so the OS can hook threads to the exception-related
	;; part of the idt, and handle exceptions in any way he wants.

bits 32
	
global idt_call_table	
global exceptions_call_table	
global int_code_start
global int_run
global int_14

global int_32 ;; remove!

global no_copro
		
extern exc_error_code
extern exc_int_addr
	
extern handle_int

extern k_scr_clear
extern k_scr_print
extern k_scr_hex
extern arch_dump_cpu	
extern curr_thread
extern curr_task

extern arch_detected_mmxfpu
extern caps_exception

extern stack_winding_int
extern stack_unwind_int

%define KRN_DATA 0x10

%define NOT_TRAP_FLAG  0xFFFFFEFF
%define TRAP_FLAG      0x100
	
%macro int_hook 1
	call stack_winding_int
	push dword %1
    jmp int_run
%endmacro

%macro int_hook_pop_error 1
	push ds
	push eax

	mov eax, 0x10
	mov ds, eax
		
	mov eax, [esp+8]
	mov [exc_error_code], eax

	mov eax, [esp]
	mov [esp+8], eax	; set error code position with eax val
	pop eax             ; ignore our eax push (dont use add for it touches the flags)
    pop ds
	pop eax             ; now eax has its original value which was stored on esp+8
	
	call stack_winding_int
    push dword %1
	jmp int_run
%endmacro

%macro int_hook_pop_error_addr 1
	push ds
	push eax

	mov eax, 0x10
	mov ds, eax
	
    mov eax, [esp+12]
	mov [exc_int_addr], eax
	mov eax, [esp+8]
	mov [exc_error_code], eax

	mov eax, [esp]
	mov [esp+8], eax	; set error code position with eax val
	pop eax             ; ignore our eax push (dont use add for it touches the flags)
    pop ds
	pop eax             ; now eax has its original value which was stored on esp+8
	
	call stack_winding_int
    push dword %1
	jmp int_run
%endmacro

%macro int_hook_get_addr 1
	push ds
	push eax
    
	mov eax, 0x10
	mov ds, eax
		
	mov eax, [esp+8]
	mov [exc_int_addr], eax

	pop eax
	pop ds
	
	call stack_winding_int
    push dword %1
	jmp int_run
%endmacro

int_code_start:
	
int_0:  int_hook_get_addr       0   ; divide error
int_1:  int_hook_get_addr       1   ; debug exception	
int_2:  int_hook                2   ; NMI interrupt
int_3:  int_hook_get_addr       3   ; breakpoint exception
int_4:  int_hook_get_addr       4   ; overflow exception
int_5:  int_hook_get_addr       5   ; BOUND range exceeded exception
int_6:	int_hook_get_addr       6   ; invalid opcode exception
int_7:  int_hook_get_addr       7   ; device not available exception
int_8:  int_hook_pop_error      8	; double fault exception
int_9:  int_hook_get_addr       9   ; copro segment overrun
int_10: int_hook_pop_error      10  ; invalid TSS exception
int_11: int_hook_pop_error      11  ; segment not present
int_12:	int_hook_pop_error_addr 12  ; stack_fault_exception
int_13: int_hook_pop_error_addr 13  ; general protection fault
int_14: int_hook_pop_error      14  ; page-fault exception
int_15:	int_hook                15  ; reserved
int_16:	int_hook_get_addr       16	; floating-point error exception
int_17:	int_hook_pop_error      17	; alignment check exception
int_18: int_hook_get_addr       18	; machine check exception
int_19: int_hook_get_addr       19	; SIMD floating-point exception
%assign i 20
%rep    234
	global int_%[i]
     int_%[i]: int_hook i   
%assign i i+1
%endrep

	;; that was terrible, but the only way to know which interrupt we
	;; are handling is through the entry point. so i think it's the
	;; only way. 

int_run:
	pop eax			; eax <- int number

	push ds
	push es
	
	mov ecx, KRN_DATA
	mov ds, ecx
	mov es, ecx
	
	push eax
	call handle_int
	
    add esp, 4
    pop es
	pop ds
	
    call stack_unwind_int
	iret
 
idt_call_table:	
%assign i 0
%rep    234
     dd int_%[i]
%assign i i+1
%endrep
dd default_int		;; spurious will be mapped here

	;; that was all the interrupt handling. what follows is the
	;; default handlers for the i386 exceptions. they just
	;; print a dump and halt the system.
		
unex:
	push dword 0    ; error is 0
	pusha
	push ds
	push es
	mov esi, unex_msg
	jmp exceptional_death
div_err:
	push dword 0    ; error is 0
	pusha
	push ds
	push es
	mov esi, div_err_msg
	jmp exceptional_death
nmi:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, nmi_msg
	jmp exceptional_death
break_point:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, break_point_msg
	jmp exceptional_death
overflow:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, overflow_msg
	jmp exceptional_death
bound_exc:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, bound_exc_msg
	jmp exceptional_death
inv_opc:
	push dword 0    ; error is 0
	pusha
	push ds
	push es	
	mov esi, inv_opc_msg
	jmp exceptional_death
dbl_fault:
	pusha
	push ds
	push es	
	mov esi, dbl_fault_msg
	jmp exceptional_death
cpr_seg_overr:
	push dword 0    ; error is 0
	pusha
	push ds
	push es	
	mov esi, cpr_seg_overr_msg
	jmp exceptional_death
inv_tss:
	pusha
	push ds
	push es	
	mov esi, inv_tss_msg
	jmp exceptional_death
seg_not_present:
	pusha
	push ds
	push es	
	mov esi, seg_not_present_msg
	jmp exceptional_death
stack_fault:
	pusha
	push ds
	push es
	mov esi, stack_fault_msg
	jmp exceptional_death
gen_prot:
	pusha
	push ds
	push es
	mov esi, gen_prot_msg
	jmp exceptional_death
page_fault:
	pusha
	push ds
	push es
	mov esi, page_fault_msg
	jmp exceptional_death
float_error:
	push dword 0    ; error is 0
	pusha
	push ds
	push es	
	mov esi, float_error_msg
	jmp exceptional_death
align_check:
	pusha
	push ds
	push es
	mov esi, align_check_msg
	jmp exceptional_death
machine_check:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, machine_check_msg
	jmp exceptional_death		
simd_ext:	
	xor eax, eax
	push eax    ; error is 0
	pusha	
	push ds
	push es
	mov esi, simd_ext_msg
	jmp exceptional_death
	
extern int7handler
extern int1handler

;; we will handle this interrupt here first and check 
;; if the processor is stepping. If it is, we will handle 
;; the int until we return to userland.
;; stack: 
;; [(ss),(esp)], eflags, (original cs), (eip for ret), eax, ebx
debug:
    push eax
    push ebx
    mov eax, dr6
    and eax, 0x4000
    jz debug_not_stepping
    ;; are we on sartoris code??
    ;; get cs from the stack
    mov eax, [esp + 12]
    cmp eax, 8
    je debug_sartoris                          ;; next instruction is on saroris code, continue stepping
debug_not_stepping:
    push ds
    mov ebx, KRN_DATA
	mov ds, ebx
    cmp dword [int1handler], 0
    pop ds
    pop ebx
    pop eax
    je debug_error
    call stack_winding_int
    push ds
	push es
    mov ecx, KRN_DATA
	mov ds, ecx
	mov es, ecx
    ;; call handle int
	push dword 1
	call handle_int
	add esp, 4
    pop es
	pop ds
    call stack_unwind_int
    iret
debug_sartoris:
    ;; set the trap flag again
    mov eax, [esp+16]                          ;; get the flags from the stack
    or eax, TRAP_FLAG
    mov dword [esp+16], eax                    ;; popf will have the step flag
    pop ebx
    pop eax
    iret                                       ;; next trap will be on the instruction that follows (intel docs says so)
debug_error:
	push dword 0    ; error is 0
	pusha	
	push ds
	push es
	mov esi, debug_msg
	jmp exceptional_death
	
;; since we implemented software task switching and
;; mmx fpu sse support, this exception will be handled first by us
bits 32
no_copro:
    call stack_winding_int
	
	mov ecx, KRN_DATA
	mov ds, ecx
	mov es, ecx
	
	call caps_exception
	cmp eax, 1
	je no_copro_present
	cmp eax, 2
	je no_copro_end

	call arch_detected_mmxfpu
	cmp eax, 0
	jz no_copro_end
no_copro_present:

	cmp dword [int7handler], 0
	je no_copro_die
	
	;; call handle int 	
	push dword 7
	call handle_int
	add esp, 4
	
	call stack_unwind_int
	iret
	
no_copro_die:
	;; no mmx/fpu support
	;; raise kernel exception	
	mov esi, no_copro_msg
	jmp exceptional_death
	
no_copro_end:
	call stack_unwind_int
	iret
	
global default_int
bits 32
default_int:
	;; do nothing (this is to avoid exceptions of spurious ints)	
	iret
	
;; void exceptional_death(int hasError)
exceptional_death:
xchg bx,bx		
	mov eax, 0x10
	mov ds, eax
	mov es, eax

	call k_scr_clear
	
	push dword 0x7
	push dword err_msg
	call k_scr_print
	add esp, 4
	
	push esi
	call k_scr_print
	add esp, 4

	push dword msg2
	call k_scr_print
	add esp, 4

	push dword [curr_thread]
	call k_scr_hex
	add esp, 4

	push dword msg3
	call k_scr_print
	add esp, 4

	push dword [curr_task]
	call k_scr_hex
	add esp, 4

	push dword msg4
	call k_scr_print
	add esp, 8
	
	call arch_dump_cpu	
	mov ebp, [esp]		
	mov ebx, [esp+4]	
	mov esi, [esp+8]
	mov edi, [esp+12]

	push dword 0x7
	push dword msg5
	call k_scr_print
	add esp, 4
	
	push ebp
	call k_scr_hex
	add esp, 4
	
	push dword msg6
	call k_scr_print
	add esp, 4	
	
	push ebx
	call k_scr_hex
	add esp, 4
	
	push dword msg6
	call k_scr_print
	add esp, 4
	
	push esi
	call k_scr_hex
	add esp, 4
	
	push dword msg6
	call k_scr_print
	add esp, 4
	
	push edi
	call k_scr_hex
	add esp, 8
	
die:	
	jmp die	


err_msg: 
db 0xa, "    the kernel has died!", 0xa, 0xa, "    obituary:", 0xa, 0	
div_err_msg:
db 0xa, "    exception 0 (divide error)", 0
debug_msg:
db 0xa, "    exception 1 (debug)", 0
nmi_msg:
db 0xa, "    exception 2 (non maskable interrupt)", 0
break_point_msg:
db 0xa, "    exception 3 (breakpoint)", 0
overflow_msg:
db 0xa, "    exception 4 (overflow)", 0
bound_exc_msg:
db 0xa, "    exception 5 (BOUND range exceeded)", 0
inv_opc_msg:
db 0xa, "    exception 6 (invalid opcode)", 0
no_copro_msg:
db 0xa, "    exception 7 (device not available, no math coprocessor)", 0
dbl_fault_msg:	
db 0xa, "    exception 8 (double fault)", 0
cpr_seg_overr_msg:
db 0xa, "    exception 9 (coprocessor segment overrun)", 0
inv_tss_msg:	
db 0xa, "    exception 10 (invalid TSS)", 0
seg_not_present_msg:	
db 0xa, "    exception 11 (segment not present)", 0
stack_fault_msg:	
db 0xa, "    exception 12 (stack-segment fault)", 0
gen_prot_msg:	
db 0xa, "    exception 13 (general protection)", 0
page_fault_msg:
db 0xa, "    exception 14 (page fault)", 0
float_error_msg:
db 0xa, "    exception 16 (floating point error)", 0
align_check_msg:
db 0xa, "    exception 17 (alignament check)", 0
machine_check_msg:	
db 0xa, "    exception 18 (machine check)", 0
simd_ext_msg:
db 0xa, "    exception 19 (streaming SIMD extensions)", 0	
unex_msg:
db 0xa, "    unexpected exception", 0
msg2:	db " received while running", 0xa, "    thread ", 0
msg3:	db " of task ", 0
msg4:	db ".", 0xa, "    register dump:", 0xa, 0
msg5:	db 0xa, 0xa, "stack trace:", 0xa, 0	
msg6:	db ", ", 0


exceptions_call_table:
dd div_err, debug, nmi, break_point, overflow, bound_exc, inv_opc, no_copro, dbl_fault, cpr_seg_overr, inv_tss, seg_not_present, stack_fault, gen_prot, page_fault, unex, float_error, align_check, machine_check, simd_ext
times 12 dd unex
	




