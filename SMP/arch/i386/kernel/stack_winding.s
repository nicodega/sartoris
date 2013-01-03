
	;; Sartoris i386 interrupt support
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com
	
	;; On some systemcalls and on interrupts, sartoris will preserve
	;; the application registers on it's stack. This is performed
	;; this way so we can implement debugging capabilities for the OS 
	;; on top. We will preserve eax, ecx, edx, ebx, esp, ebp, esi, edi,
	;; eip prior to the interrupt or syscall, segment registers and flags.

bits 32

%include "defines.inc"

global stack_winding_int
global stack_unwind_int
global stack_winding_syscall
global stack_unwind_syscall

extern mmx_state_owner
extern curr_thread
extern arch_caps
extern curr_state

;; NOTE: we should only invoke this function on interrupts
;; when the int was not raised on sartoris memory. 
;; (i.e. the process was not executing sartoris code)

;; Intra-privilege level call
;; On the new stack, the processor pushes the segment selector and stack pointer for the calling procedure's stack, an (optional) 
;; set of parameters from the calling procedures stack, and the segment selector and instruction pointer for the calling procedure's 
;; code segment. (A value in the call gate descriptor determines how many parameters to copy to the new stack.) Finally, the processor 
;; branches to the address of the procedure being called within the new code segment.

;; intra privilege level interrupt

;; stack will contain (on exit):
;; higher mem  ---->  lower
;; [(ss),(esp)], flags, (original cs), (eip for ret), space for ret eip, flags (winding), eax, ecx, edx, ebx, esp, ebp, esi, edi, 
;; d0, d1, d2, d3, d6, d7
;; eip, cs, fs, gs, es, ds, ss

stack_winding_int:
    pushf
	pushad
    mov esi, ds                             ;;  we need sartoris ds segment to read curr_state we will keep it on esi
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ecx, [curr_state]
    ;; if we where on sartoris code when the int happened, 
    ;; increment sints (we must do this allways, because
    ;; tracing could be enabled when the thread was interrupted 
    ;; by an interrupt on sartoris code)
    mov edx, [ecx+(16*4)]
    cmp dword [esp+(11*4)], 8              ;; sartoris area must be safe!
    jne stack_winding_int_cont_nosint
    mov eax, [ecx+(16*4)]
    inc eax
    mov edx, eax
    mov dword [ecx+(16*4)], eax
    mov eax, [esp+(9*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    mov ds, esi
    ret
stack_winding_int_cont_nosint:
    mov ebx, [ecx]
    mov ecx, ebx
    and ebx, SFLAG_TRACEABLE
    jnz stack_winding_int_cont
    mov eax, [esp+(9*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    mov ds, esi
    ret
stack_winding_int_cont:
    ;; if this thread is the mmx owner, preserve it's state
    mov eax, [curr_thread]
    cmp eax, [mmx_state_owner]
    jne stack_winding_int_no_mmx_save
    mov eax, [arch_caps]
	and eax, 0x10					        ;; this is SCAP_FXSR
	mov ebx, [curr_state]
	jz stack_winding_int_no_fxsave
    fxsave [ebx + thr_state.mmx]
	jmp stack_winding_int_no_mmx_save
stack_winding_int_no_fxsave:
	fsave [ecx + thr_state.mmx]
stack_winding_int_no_mmx_save:
    mov eax, [esp+(12*4)]                   ;; get the original flags from the stack
    mov dword [esp+(8*4)], eax              ;; put them where we pushed the flags
    and ecx, SFLAG_DBG
    jz stack_winding_int_nodbg
    mov eax, dr0
    push eax
    mov eax, dr1
    push eax
    mov eax, dr2
    push eax
    mov eax, dr3
    push eax
    mov eax, dr6
    push eax
    mov eax, dr7
    push eax
    jmp stack_winding_int_dbg_cont
stack_winding_int_nodbg:
    sub esp, (4*4)
    push dword 0x400
    push dword 0xFFFF0FF0
stack_winding_int_dbg_cont:
	mov eax, [esp+(16*4)] ;; get eip (account for pushed registers and the error code)
	push eax
	mov eax, [esp+(18*4)] ;; get cs (account for pushed registers and the error code)
	push eax
	;; the stack now contains the eip and common registers
	push fs
	push gs
	push es
	push esi                                ;; esi contains old ds
    ;; get ss from the stack
    mov eax, dword [esp+(26*4)]             ;; get the ss from the stack before privilege level switch
	push eax
    mov eax, [esp+(26*4)]                   ;; get esp from the int call
    mov dword [esp+(16*4)], eax             ;; place the right esp on it's stack possition
	mov eax, esp
    mov ebx, [curr_state]
	mov dword [ebx + thr_state.stack_winding], eax		        ;; put the stack winding pointer on thread_state
    mov eax, [esp+(22*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
    mov ds, esi
	ret
    
;; stack will contain (on exit):
;; higher mem  ---->  lower
;; [ss, esp, [... params ...]], 
;; (original cs), (eip for ret), params count, space for ret eip/caller eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; d0, d1, d2, d3, d6, d7
;; eip, cs, fs, gs, es, ds, ss

stack_winding_syscall:
    pushf
    ;; make sure we won't be stepping on kernel code
    cli
    push ebx
    push ecx
    push ds
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ebx, [curr_state]
    mov ecx, [ebx]
    and ecx, SFLAG_TRACEABLE
    jnz stack_winding_syscall_cont
    pop ds
    pop ecx
    pop ebx
    mov eax, 0            ;; on eax we will return memory size of the winding
    popf
    ret
stack_winding_syscall_cont:
    ;; if this thread is the mmx owner, preserve it's state
    mov ecx, [curr_thread]
    cmp ecx, [mmx_state_owner]
    jne stack_winding_syscall_no_mmx_save
    mov ecx, [arch_caps]
	and ecx, 0x10					;; this is SCAP_FXSR
	jz stack_winding_syscall_no_fxsave
    fxsave [ebx + thr_state.mmx]
	jmp stack_winding_syscall_no_mmx_save
stack_winding_syscall_no_fxsave:
	fsave [ebx + thr_state.mmx]
stack_winding_syscall_no_mmx_save:
    pop ds
    pop ecx               ;; pop the ecx value we preserved before
    pop ebx               ;; pop the ebx value we preserved before
	pushad
    mov esi, ds
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ebx, [curr_state]
    mov ecx, [ebx]
    and ecx, SFLAG_DBG
    jz stack_winding_syscall_nodbg
    mov eax, dr0
    push eax
    mov eax, dr1
    push eax
    mov eax, dr2
    push eax
    mov eax, dr3
    push eax
    mov eax, dr6
    push eax
    mov eax, dr7
    push eax
    jmp stack_winding_syscall_dbg_cont
stack_winding_syscall_nodbg:
    sub esp, (4*4)
    push dword 0x400
    push dword 0xFFFF0FF0
stack_winding_syscall_dbg_cont:
	mov eax, [esp+(17*4)] ;; get eip
	push eax
	mov eax, [esp+(19*4)] ;; get cs
	push eax
	;; the stack now contains the eip and common registers
	push fs
	push gs
	push es
	push esi                                ;; esi contains old ds
    ;; to get esp and ss we need to know how many
    ;; parameters the syscall has. 
    mov eax, [esp+(22*4)]
    add eax, 24                           
    shl eax, 2
    mov ecx, esp
    add ecx, eax
    mov eax, [ecx + 8]
	push eax                                ;; push ss
    mov eax, [ecx + 4]
    mov dword [esp+(16*4)], eax             ;; place the right esp on it's stack possition
	mov eax, esp
	mov dword [ebx + thr_state.stack_winding], eax	;; put the stack winding pointer on thread_state
    ;; restore interrupts state
    mov eax, [esp+(21*4)]
    push eax
    popf
    mov eax, [esp+(22*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
    mov ds, esi
    mov eax, 21                             ;; on eax we will return memory size of the winding
	ret
    
;; stack will contain:
;; higher mem  ---->  lower
;; (ss before int),(esp before int), iret flags, (original cs), (eip for ret), space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; d0, d1, d2, d3, d6, d7
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

stack_unwind_int:
    mov esi, ds                    ;;  we need sartoris ds segment to read curr_state we will keep it on esi
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ebx, [curr_state]
    ;; where we on sartoris code? -> decrement sints and return without unwinding (just popa, popf)
    mov eax, [ebx + (16*4)]
    cmp eax, 0                     ;; where whe on a sartoris int?
    je stack_unwind_int_nosint
    dec eax
    mov dword [ebx + (16*4)], eax
    jmp stack_unwind_int_ret_nounwind
stack_unwind_int_nosint:
    ;; do we have a winding?
    cmp dword [ebx + thr_state.stack_winding], 0
    je stack_unwind_int_ret_nounwind        ;; stack_winding = NULL
    mov edx, [ebx]
    mov ebx, edx
    mov ecx, ebx
    and edx, (SFLAG_TRACE_END|SFLAG_TRACEABLE)  ;; if tracing is ending, we must remove the winding
    jz stack_unwind_int_ret_nounwind
    ;; remove end mark if it was set
    and ecx, ~SFLAG_TRACE_END
    mov eax, [curr_state]
    mov dword [eax], ecx
    ;; move this function return address to the error code
    ;; space (or the space we left on non-ec int/syscall winding)
    pop eax
    mov dword [esp+(22*4)], eax
    pop eax                         ;; get ss
    mov dword [esp+(26*4)], eax
    mov eax, [esp+(15*4)]           ;; get esp
    mov dword [esp+(25*4)], eax
    mov eax, [esp+(20*4)]           ;; get flags
	and eax, ~TRAP_FLAG             ;; remove stepping if it was set (it'll be set on iret)
    mov dword [esp+(24*4)], eax     ;; set flags
    pop edi                         ;; ignore ds
	pop es
	pop gs
	pop fs
	pop eax							;; get cs
	mov dword [esp+(18*4)], eax	    ;; put cs
	pop eax							;; get eip
	mov dword [esp+(16*4)], eax	    ;; put eip
    and ecx, SFLAG_DBG
    jz stack_unwind_int_nodbg
    pop eax
    mov dr0, eax
    pop eax
    mov dr1, eax
    pop eax
    mov dr2, eax
    pop eax
    mov dr3, eax
    pop eax
    mov dr6, eax
    pop eax
    mov dr7, eax
    jmp stack_unwind_int_dbg_cont
stack_unwind_int_nodbg:
    add esp, 6*4
stack_unwind_int_dbg_cont:
    xor eax, eax
    mov ebx, [curr_state]
	mov dword [ebx + thr_state.stack_winding], eax
    mov ds, esi
	popad
    mov dword [esp], eax            ;; remove the flags from the stack without affecting any register
    pop eax
    ret
stack_unwind_int_ret_nounwind:
    pop eax                        ;; get the caller eip
    mov dword [esp+(9*4)], eax     ;; place it on the free space
    mov ds, esi
    popad                          ;; restore all registers
    popf
    ret

;; stack will contain:
;; higher mem  ---->  lower
;; [ss, esp, [... params ...]], 
;; (original cs), (eip for ret), params count, space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; d0, d1, d2, d3, d6, d7
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

;; we should preserve eax here, because it contains the syscall return value.
;; we should also stick to C calling convention register preservation.
stack_unwind_syscall:
    pushf
    push ebx
    cli
    mov ecx, ds                             ;; we need sartoris ds segment to read curr_state.
    mov ebx, KRN_DATA
    mov ds, ebx
    mov ebx, [curr_state]
    cmp dword [ebx + thr_state.stack_winding], 0
    je stack_unwind_syscall_nowinding
    mov edx, [ebx]
    mov ebx, edx
    and ebx, (SFLAG_TRACE_END|SFLAG_TRACEABLE)
    jnz stack_unwind_syscall_cont
stack_unwind_syscall_nowinding:
    mov ds, ecx
    pop ebx                                 ;; restore ebx! (C calling convention requirement)
    popf                                    ;; this will enable ints again if they where enabled
    ret
stack_unwind_syscall_cont:
    add esp, 8                              ;; remove ebx and eflags from the stack, we will get it on popad and popf
    ;; remove end mark if it was set
    and edx, ~SFLAG_TRACE_END
    mov ebx, [curr_state]
    mov dword [ebx], edx
    mov dword [esp+(21*4)], eax         ;; change eax value on the winding, so we can use it here
    ;; move this function return address to the error code
    ;; space (or the space we left on non-ec int/syscall winding)
    pop ebx
    mov dword [esp+(22*4)], ebx
    ;; on ebx we will place syscall's first param address on the stack
    mov ebx, dword [esp+(23*4)]
    add ebx, 25
    shl ebx, 2
    add ebx, esp
    mov eax, [esp]                  ;; get ss
    mov dword [ebx + 8], eax
    mov eax, [esp+(16*4)]           ;; get esp
    mov dword [ebx + 4], eax
    ;; restore registers
    pop eax                         ;; ignore ss
	pop eax                         ;; ignore ds (it's on ecx)
	pop es
	pop gs
	pop fs
	pop eax							;; get cs
	mov dword [esp+(19*4)], eax	    ;; put cs
	pop eax							;; get eip
	mov dword [esp+(17*4)], eax	    ;; put eip
    and edx, SFLAG_DBG
    jz stack_unwind_syscall_nodbg
    pop eax
    mov dr0, eax
    pop eax
    mov dr1, eax
    pop eax
    mov dr2, eax
    pop eax
    mov dr3, eax
    pop eax
    mov dr6, eax
    pop eax
    mov dr7, eax
    jmp stack_unwind_syscall_dbg_cont
stack_unwind_syscall_nodbg:
    add esp, 6*4
stack_unwind_syscall_dbg_cont:
    xor eax, eax
    mov ebx, [curr_state]
	mov dword [ebx + thr_state.stack_winding], eax
    mov ds, ecx                     ;; restore original ds
    popad                           ;; esp is discarded! yay!!
    popf                            ;; if interrupts where enabled this will restore them
	ret
