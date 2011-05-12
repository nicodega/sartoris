
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

global no_copro
		
extern exc_error_code
	
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

extern curr_state

%define KRN_DATA 0x10

%define NOT_TRAP_FLAG  0xFFFFFEFF
%define TRAP_FLAG      0x100

;; REMEMBER: This code is executed on privilege 0!
;; that's why we can wind and unwind freely.
%macro int_hook
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

	;; that was terrible, but the only way to know which interrupt we
	;; are handling is through the entry point. so i think it's the
	;; only way. maybe there is a way to do this with the assembler,
	;; but i don't know how to.

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

int_code_start:
	
int_0:  int_hook 0		; divide error
int_1:  int_hook 1		; debug exception	
int_2:  int_hook 2		; NMI interrupt
int_3:  int_hook 3		; breakpoint exception
int_4:  int_hook 4		; overflow exception
int_5:  int_hook 5		; BOUND range exceeded exception
int_6:	int_hook 6		; invalid opcode exception
int_7:  int_hook 7		; device not available exception
int_8:  int_hook_pop_error 8	; double fault exception
int_9:  int_hook 9		; copro segment overrun
int_10: int_hook_pop_error 10	; invalid TSS exception
int_11: int_hook_pop_error 11	; segment not present
int_12:	int_hook_pop_error 12	; stack_fault_exception
int_13: int_hook_pop_error 13	; general protection fault
int_14: int_hook_pop_error 14	; page-fault exception
int_15:	int_hook 15		; reserved
int_16:	int_hook 16		; floating-point error exception
int_17:	int_hook_pop_error 17	; alignament check exception
int_18: int_hook 18		; machine check exception
int_19: int_hook 19		; SIMD floating-point exception
int_20: int_hook 20
int_21: int_hook 21
int_22: int_hook 22
int_23: int_hook 23
int_24: int_hook 24
int_25: int_hook 25
int_26: int_hook 26
int_27: int_hook 27
int_28: int_hook 28
int_29: int_hook 29
int_30: int_hook 30
int_31: int_hook 31
int_32: int_hook 32
int_33: int_hook 33
int_34: int_hook 34
int_35: int_hook 35
int_36: int_hook 36
int_37: int_hook 37
int_38: int_hook 38
int_39: int_hook 39
int_40: int_hook 40
int_41: int_hook 41
int_42: int_hook 42
int_43: int_hook 43
int_44: int_hook 44
int_45: int_hook 45
int_46: int_hook 46
int_47: int_hook 47
int_48: int_hook 48
int_49: int_hook 49
int_50: int_hook 50
int_51: int_hook 51
int_52: int_hook 52
int_53: int_hook 53
int_54: int_hook 54
int_55: int_hook 55
int_56: int_hook 56
int_57: int_hook 57
int_58: int_hook 58
int_59: int_hook 59
int_60: int_hook 60
int_61: int_hook 61
int_62: int_hook 62
int_63: int_hook 63

idt_call_table:	
dd int_0
dd int_1
dd int_2
dd int_3
dd int_4
dd int_5
dd int_6
dd int_7
dd int_8
dd int_9
dd int_10
dd int_11
dd int_12
dd int_13
dd int_14
dd int_15
dd int_16
dd int_17
dd int_18
dd int_19
dd int_20
dd int_21
dd int_22
dd int_23
dd int_24
dd int_25
dd int_26
dd int_27
dd int_28
dd int_29
dd int_30
dd int_31
dd int_32
dd int_33
dd int_34
dd int_35
dd int_36
dd int_37
dd int_38
dd int_39
dd int_40
dd int_41
dd int_42
dd int_43
dd int_44
dd int_45
dd int_46
dd int_47
dd int_48
dd int_49
dd int_50
dd int_51
dd int_52
dd int_53
dd int_54
dd int_55
dd int_56
dd int_57
dd int_58
dd int_59
dd int_60
dd int_61
dd int_62
dd int_63	


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
debug:
    pushf
    push eax
    push ebx
    mov eax, dr6
    and eax, 0x4000
    jz debug_not_stepping
    ;; are we on sartoris code??
    ;; get the eip from the stack
    mov eax, [esp + (12*4)]
    cmp eax, 0x7000000
    jb debug_sartoris                          ;; next instruction is on saroris code, continue stepping
debug_not_stepping:
    pop ebx
    pop eax
    popf
    cmp dword [int1handler], 0
    je debug_error
    call stack_winding_int
    ;; call handle int 	
	push dword 7
	call handle_int
	add esp, 4
    call stack_unwind_int
    iret
debug_sartoris:
    ;; set the trap flag again
    mov eax, [esp+12]                          ;; get the flags from the stack
    or eax, TRAP_FLAG
    mov dword [esp+12], eax                    ;; popf will have the step flag
    pop ebx
    pop eax
    popf
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
	




