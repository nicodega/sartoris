
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

extern curr_state

%define SFLAG_TRACEABLE         0x8
%define SFLAG_DBG               0x10
%define SFLAG_TRACE_END         0x40
%define NOT_SFLAG_TRACE_END     0xFFFFFFBF

global stack_winding_int
global stack_unwind_int
global stack_winding_syscall
global stack_unwind_syscall

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
;; (ss),(esp),(original cs), (eip for ret), space for ret eip, pushf, eax, ecx, edx, ebx, esp, ebp, esi, edi, 
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss

stack_winding_int:
    pushf
	pushad
    mov ecx, [curr_state]
    mov ebx, ecx
    and ebx, SFLAG_TRACEABLE
    jnz stack_winding_int_cont
    mov eax, [esp + (8*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    ret
stack_winding_int_cont:
    ;; if we are on sartoris code, we should not wind the stack
    cmp dword [esp +(8*4)], 0x7000000       ;; sartoris area must be safe!
    jae stack_winding_int_cont2
    mov ebx, curr_state                     ;; increment sints on thread_state
    mov eax, [ebx + (16*4)]
    inc eax
    mov dword [ebx + (16*4)], eax
    mov eax, [esp + (8*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    ret
stack_winding_int_cont2:
    xor edx, edx
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
    mov edx, (6*4)
stack_winding_int_nodbg:
	mov eax, [esp+edx+(10*4)] ;; get eip (account for pushed registers and the error code)
	push eax
	mov eax, [esp+edx+(11*4)] ;; get cs (account for pushed registers and the error code)
	push eax
	;; the stack now contains the eip and common registers
	push fs
	push gs
	push es
	push ds
    ;; get ss from the stack
    mov eax, dword [esp+edx+(19*4)]         ;; get the ss from the stack before privilege level switch
	push eax
    mov eax, [esp+edx+(10*4)]
    mov dword [esp+edx+(10*4)], eax         ;; place the right esp on it's stack possition
	pushf
	mov eax, esp
	add eax, 16*4							;; eax now has the start of the stack where we begun pushing
    mov ebx, curr_state
	mov dword [ebx + 60], eax		        ;; put the stack winding pointer on thread_state
    mov eax, [esp+ edx + (16*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
	ret
    
;; stack will contain (on exit):
;; higher mem  ---->  lower
;; [ss, esp, [... params ...]], 
;; (original cs), (eip for ret), params count, space for ret eip/caller eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss

stack_winding_syscall:
    pushf
    cli
    push ebx
    mov ebx, [curr_state]
    and ebx, SFLAG_TRACEABLE
    jnz stack_winding_syscall_cont
    pop ebx
    popf
    ret
stack_winding_syscall_cont:
    pop ebx               ;; pop the ebx value we preserved before
	pushad
    xor edx, edx
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
    mov edx, (6*4)
stack_winding_syscall_nodbg:
	mov eax, [esp+edx+(11*4)] ;; get eip
	push eax
	mov eax, [esp+edx+(12*4)] ;; get cs
	push eax
	;; the stack now contains the eip and common registers
	push fs
	push gs
	push es
	push ds
    ;; to get esp and ss we need to know how many
    ;; parameters the syscall has. 
    mov eax, [esp+edx+(16*4)]
    add eax, 17                             ;; (not 18 since flags will be poped)
    shl eax, 2
    mov ebx, esp
    add ebx, eax
    mov eax, [ebx + 8]
	push eax                                ;; push ss
    mov eax, [ebx + 4]
    mov dword [esp+edx+(10*4)], eax         ;; place the right esp on it's stack possition
	mov eax, esp
	add eax, 14*4							;; eax now has the start of the stack where we begun pushing
	mov ebx, curr_state
	mov dword [ebx + 60], eax		        ;; put the stack winding pointer on thread_state
    ;; restore interrupts state
    mov eax, [esp+edx+(15*4)]
    push eax
    popf
    mov eax, [esp+edx+(16*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
	ret
    
;; stack will contain:
;; higher mem  ---->  lower
;; (ss before int),(esp before int),(original cs), (eip for ret), space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

stack_unwind_int:
    mov ebx, [curr_state]
    mov edx, ebx
    mov ecx, ebx
    and edx, SFLAG_TRACE_END
    jnz stack_unwind_int_cont
    and ebx, SFLAG_TRACEABLE
    jnz stack_unwind_int_cont
    pop eax                        ;; get the caller eip
    mov dword [esp + (9*4)], eax   ;; place it on the free space
    popad                          ;; restore all registers
    popf
    ret                
stack_unwind_int_cont:
    ;; if we are on sartoris code, we should not wind the stack
    mov ebx, curr_state
    mov eax, [ebx + (16*4)]
    cmp eax, 0
    je stack_unwind_int_cont2
    ;; decrement sints on curr_state and return
    dec eax
    mov dword [ebx + (16*4)], eax
    pop eax                        ;; get the caller eip
    mov dword [esp + (9*4)], eax   ;; place it on the free space
    popad
    popf
    ret
stack_unwind_int_cont2:
    ;; remove end mark if it was set
    and ecx, NOT_SFLAG_TRACE_END
    mov dword [curr_state], ecx
    xor edx, edx
    and ecx, SFLAG_DBG
    jz stack_unwind_int_nodbg
    mov edx, (6*4)
stack_unwind_int_nodbg:
    ;; move this function return address to the error code
    ;; space (or the space we left on non-ec int/syscall winding)
    pop eax
    mov dword [esp+edx+(16*4)], eax
    pop eax                         ;; get ss
    mov dword [esp+edx+(19*4)], eax
    mov eax, [esp+edx+(9*4)]        ;; get esp
    mov dword [esp+edx+(18*4)], eax
	pop ds
	pop es
	pop gs
	pop fs
	pop eax							;; get cs
	mov dword [esp+edx+(12*4)], eax	;; put cs
	pop eax							;; get eip
	mov dword [esp+edx+(10*4)], eax	;; put eip
    cmp edx, 0
    je stack_unwind_int_nodbg2
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
stack_unwind_int_nodbg2:
    xor eax, eax
    mov ebx, curr_state
	mov dword [ebx + 60], eax
	popad
    popf
    ret

;; stack will contain:
;; higher mem  ---->  lower
;; [ss, esp, [... params ...]], 
;; (original cs), (eip for ret), params count, space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

stack_unwind_syscall:
    mov edx, ebx
    mov ecx, ebx
    and edx, SFLAG_TRACE_END
    jnz stack_unwind_syscall_cont
    and ebx, SFLAG_TRACEABLE
    jnz stack_unwind_syscall_cont
    ret
stack_unwind_syscall_cont:
    cli
    xor edx, edx
    and ecx, SFLAG_DBG
    jz stack_unwind_int_nodbg
    mov edx, (6*4)
stack_unwind_syscall_nodbg:
    ;; remove end mark if it was set
    and ecx, NOT_SFLAG_TRACE_END
    mov dword [curr_state], ecx
    ;; move this function return address to the error code
    ;; space (or the space we left on non-ec int/syscall winding)
    pop eax
    mov dword [esp+edx+(17*4)], eax
    ;; on ebx we will place syscall's first param address on the stack
    mov ebx, dword [esp+edx+(18*4)]
    add ebx, 19
    add ebx, edx
    shl ebx, 2
    mov eax, [esp]                  ;; get ss
    mov dword [ebx + 8], eax
    mov eax, [esp+edx+(10*4)]      ;; get esp
    mov dword [ebx + 4], eax
    ;; restore registers
    pop eax                         ;; ignore ss
	pop ds
	pop es
	pop gs
	pop fs
	pop eax							;; get cs
	mov dword [esp+edx+(13*4)], eax	;; put cs
	pop eax							;; get eip
	mov dword [esp+edx+(12*4)], eax	;; put eip
    cmp edx, 0
    je stack_unwind_syscall_nodbg2
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
stack_unwind_syscall_nodbg2:
    xor eax, eax
    mov ebx, curr_state
	mov dword [ebx + 60], eax
	popad                           ;; esp is discarded! yay!!
    popf                            ;; if interrupts where enabled this will restore them
	ret
