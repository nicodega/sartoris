
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
%define NOT_INT_ENABLE          0xFFFFFDFF          ;; not eflags IF
%define KRN_DATA                0x10

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
;; [(ss),(esp)],(original cs), (eip for ret), space for ret eip, pushf, eax, ecx, edx, ebx, esp, ebp, esi, edi, 
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss
;; forst ss, esp will not be there if we where invoked from within sartoris

stack_winding_int:
    pushf
	pushad
    mov esi, ds                             ;;  we need sartoris ds segment to read curr_state we will keep it on esi
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ecx, [curr_state]                   
    mov ebx, [ecx]
    and ebx, SFLAG_TRACEABLE
    jnz stack_winding_int_cont
    mov eax, [esp + (9*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    mov ds, esi
    ret
stack_winding_int_cont:
xchg bx,bx
    ;; if we are on sartoris code, we should not wind the stack
    cmp dword [esp +(9*4)], 0x7000000       ;; sartoris area must be safe!
    jae stack_winding_int_cont2
    mov ebx, [curr_state]                   ;; increment sints on thread_state
    mov eax, [ebx + (17*4)]
    inc eax
    mov dword [ebx + (17*4)], eax
    mov eax, [esp + (9*4)]
    push eax                                ;; push the eip who called us, in order to preserve the stack
    mov ds, esi
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
	push esi                                ;; esi contains old ds
    ;; get ss from the stack
    mov eax, dword [esp+edx+(19*4)]         ;; get the ss from the stack before privilege level switch
	push eax
    mov eax, [esp+edx+(10*4)]
    mov dword [esp+edx+(10*4)], eax         ;; place the right esp on it's stack possition
	pushf
	mov eax, esp
	add eax, 16*4							;; eax now has the start of the stack where we begun pushing
    mov ebx, [curr_state]
	mov dword [ebx + 60], eax		        ;; put the stack winding pointer on thread_state
    mov eax, [esp+edx+(16*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
    mov ds, esi
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
    popf
    mov eax, 0            ;; on eax we will return memory size of the winding
    ret
stack_winding_syscall_cont:
xchg bx,bx
    pop ds
    pop ecx               ;; pop the ecx value we preserved before
    pop ebx               ;; pop the ebx value we preserved before
	pushad
    mov esi, ds
    mov ecx, KRN_DATA
    mov ds, ecx
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
	push esi                                ;; esi contains old ds
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
	mov ebx, [curr_state]
	mov dword [ebx + 60], eax		        ;; put the stack winding pointer on thread_state
    ;; restore interrupts state
    mov eax, [esp+edx+(15*4)]
    push eax
    popf
    mov eax, [esp+edx+(16*4)]
    push eax                                ;; we push the eip who called us, in order to preserve the stack
    mov ds, esi
    mov eax, edx                               ;; on eax we will return memory size of the winding
    add eax, 17*4
	ret
    
;; stack will contain:
;; higher mem  ---->  lower
;; (ss before int),(esp before int),(original cs), (eip for ret), space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

stack_unwind_int:
    mov esi, ds                    ;;  we need sartoris ds segment to read curr_state we will keep it on esi
    mov ecx, KRN_DATA
    mov ds, ecx
    mov ebx, [curr_state]
    mov edx, [ebx]
    mov ebx, edx
    mov ecx, ebx
    and edx, SFLAG_TRACE_END
    jnz stack_unwind_int_cont
    and ebx, SFLAG_TRACEABLE
    jnz stack_unwind_int_cont
    pop eax                        ;; get the caller eip
    mov dword [esp + (9*4)], eax   ;; place it on the free space
    mov ds, esi
    popad                          ;; restore all registers
    popf
    ret                
stack_unwind_int_cont:
xchg bx,bx
    ;; if we are on sartoris code, we should not wind the stack
    mov ebx, [curr_state]
    mov eax, [ebx + (16*4)]
    cmp eax, 0
    je stack_unwind_int_cont2
    ;; decrement sints on curr_state and return
    dec eax
    mov dword [ebx + (16*4)], eax
    pop eax                        ;; get the caller eip
    mov dword [esp + (9*4)], eax   ;; place it on the free space
    mov ds, esi
    popad
    popf
    ret
stack_unwind_int_cont2:
    ;; remove end mark if it was set
    and ecx, NOT_SFLAG_TRACE_END
    mov eax, [curr_state]
    mov dword [eax], ecx
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
	pop edi                         ;; ignore ds
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
    mov ebx, [curr_state]
	mov dword [ebx + 60], eax
    mov ds, esi
	popad
    popf
    ret

;; stack will contain:
;; higher mem  ---->  lower
;; [ss, esp, [... params ...]], 
;; (original cs), (eip for ret), params count, space for ret eip, flags, eax, ecx, edx, ebx, esp, ebp, esi, edi
;; [d0, d1, d2, d3, d4, d5, d6, d7]
;; eip, cs, fs, gs, es, ds, ss, (stack_unwind ret eip)

;; we should preserve eax here, because it contains the syscall return value.
;; we should also stick to C calling convention register preservation.
stack_unwind_syscall:
    push ebx
    pushf
    cli
    mov ecx, ds                             ;; we need sartoris ds segment to read curr_state.
    mov ebx, KRN_DATA
    mov ds, ebx
    mov ebx, [esp]
    and ebx, NOT_INT_ENABLE
    popf
    mov ebx, [curr_state]
    mov edx, [ebx]
    mov ebx, edx
    and ebx, SFLAG_TRACE_END
    jnz stack_unwind_syscall_cont
    mov ebx, edx                            
    and ebx, SFLAG_TRACEABLE
    jnz stack_unwind_syscall_cont
    mov ds, ecx
    pop ebx                                 ;; restore ebx! (C calling convention requirement)
    ret
stack_unwind_syscall_cont:
xchg bx,bx
    add esp, 4                              ;; remove ebx from the stack, we will get it on popad
    ;; remove end mark if it was set
    and edx, NOT_SFLAG_TRACE_END
    mov ebx, [curr_state]
    mov dword [ebx], edx
    mov ebx, edx                            ;; now ebx has satoris flags again without NOT_SFLAG_TRACE_END
    xor edx, edx
    and ebx, SFLAG_DBG
    jz stack_unwind_int_nodbg
    mov edx, (6*4)
stack_unwind_syscall_nodbg:
    mov dword [esp+edx+(15*4)], eax         ;; change eax value on the winding, so we can use it here
    ;; move this function return address to the error code
    ;; space (or the space we left on non-ec int/syscall winding)
    pop ebx
    mov dword [esp+edx+(17*4)], ebx
    ;; on ebx we will place syscall's first param address on the stack
    mov ebx, dword [esp+edx+(18*4)]
    add ebx, 19
    add ebx, edx
    shl ebx, 2
    mov eax, [esp]                 ;; get ss
    mov dword [ebx + 8], eax
    mov eax, [esp+edx+(10*4)]      ;; get esp
    mov dword [ebx + 4], eax
    ;; restore registers
    pop eax                         ;; ignore ss
	pop eax                         ;; ignore ds (it's on ecx)
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
    mov ds, ecx                     ;; restore original ds
    popad                           ;; esp is discarded! yay!!
    popf                            ;; if interrupts where enabled this will restore them
	ret
