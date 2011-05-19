
bits 32

global arch_switch_thread
global arch_switch_thread_int
global arch_is_soft_int
global arch_detected_mmxfpu
global mmx_state_owner

extern global_tss
extern curr_state

extern arch_caps

;;
;; In order not to perform an mmx/fpu preservation always
;; because its costs us much, we will do as intel tells us
;; on its manual volume 3 :)
;;

%include "defines.inc"

;; size in bytes of thr_state
%ifdef FPU_MMX
%define THR_STATE_SIZE    (584 + STACK0_SIZE) 
%else
%define THR_STATE_SIZE    (68 + STACK0_SIZE) 
%endif

;;
;; Software based task switching support.
;;
;; void arch_switch_thread(struct thr_state *new_thread_state, int thr_id, unsigned int cr3)
arch_switch_thread:
	;; ebp, ebx, esi, and edi must be preserved
	push ebp
	mov ebp, esp
    
    ;; Preserve current thread state

	;; ds:ecx will contain thread state 
	;; mov eax,[ecx + thr_state.xxxx]
	mov ecx, [curr_state]					;; now ecx contains the state of the running thread

    ;; if we are on a soft interrupt, this means we interrupted the 
    ;; soft int (with a run_thread or an interrupt) and ge got here.
    ;; If that's the case, we must preserve our original thread 
    ;; C convension registers (ebp, esi, edi, ebx, esp)
	mov eax, [ecx + thr_state.sflags]
    mov edx, eax
	and eax, SFLAG_RUN_INT
    jz arch_switch_thread_softint_cont
    and edx, SFLAG_RUN_INT_START
    jnz arch_switch_thread_softint_cont
    ;; preserve the thread registers before it was runned again
    mov eax, [ecx + thr_state.ebp]
    push eax
    mov eax, [ecx + thr_state.esi]
    push eax
    mov eax, [ecx + thr_state.edi]
    push eax
    mov eax, [ecx + thr_state.ebx]
    push eax
    mov eax, [ecx + thr_state.esp]
    push eax
    mov eax, [ecx + thr_state.sflags]
    or eax, SFLAG_RUN_INT_STATE
    mov dword [ecx + thr_state.sflags], eax
arch_switch_thread_softint_cont:

	;; edx will contain cr3 for new thread
	mov edx, [ebp+16]			            ;; now edx contains cr3 for new thread
	
	;; preserve segment selectors (es and ds wont be saved, for they are loaded with kernel selectors)
	;; ss wont be preserved, because it was already preserved on far call to interrupt or run thread
	;; anyway, we don't need it, because we are on stack0
	mov eax, fs
	mov [ecx + thr_state.fs], eax
	mov eax, gs
	mov [ecx + thr_state.gs], eax

	;; preserve flags
	pushf
	pop eax
	mov [ecx + thr_state.eflags], eax
	
	;; preserve stack
	mov eax, esp
	mov [ecx + thr_state.esp], eax
	
	;; preserve general purpose registers now (I'll only preserve C registers here
	;; since this is a call from sartoris code)
	mov [ecx + thr_state.ebx], ebx
	mov [ecx + thr_state.esi], esi
	mov [ecx + thr_state.edi], edi
	mov [ecx + thr_state.ebp], ebp
				
	;***************************************************** 
	;			Now load new thread state
	;				
	;	 NOTE: upon ret far, we will be jumping to
	;	 the correct cs:eip for the new running thread
	;	 when we come back from arch independant code.
	;	 NOTE2: If thread has never been runned, we cannot
	;	 ret far, then, we must check first_time
	;*****************************************************
restore_state:

	;; edx contains cr3 for new thread
	;; lets load it, it does not matter
	;; the order, because we are on kernel
	;; pages
%ifdef PAGING
	;; important: Recall that for this architecture, writing to the cr3 
	;; register automatically results in a TLB flush.
	mov eax, edx
	mov cr3, eax
%endif
	
	;; Update global TSS PL 0 stack pointer for this task 
	;; stack will now be located on thread_state + sizeof(thread_state)
	;; and it's size will be STACK0_SIZE
	;; NOTE: This will only be used when 
	;; the process returns from sartoris 
	;; and a new far call is made. Thats why
	;; we dont set it to current esp. (it
	;; will also be used on first call,
	;; because we simulate a retf)
	mov eax, [ebp+8]
	add eax, (THR_STATE_SIZE - 4)
	mov ecx, global_tss
	mov [ecx+4], eax             ;; global_tss.esp0 = stack0;
		
	;; ds:ecx will contain thread state 
	mov ecx, [ebp+8]           ;; now ecx contains &thr_states[id]
	mov [curr_state], ecx      ;; update curr_state to the new one
	
	;; load ldt register
	;; indicating wich priv
	;; ldt descriptor from 
	;; gdt must be used	
	mov eax, [ecx + thr_state.ldt_sel]

	lldt [ecx + thr_state.ldt_sel] 
	
	;; restore segments (no problem again, since we are on kernel segments)
	;; ss wont be restored, because switch happens on stack0
	mov eax, [ecx + thr_state.fs]
	mov fs, eax
	mov eax, [ecx + thr_state.gs]
	mov gs, eax

    ;; in case other process is being debugged
    ;; disable debug registers
    xor eax, eax
    mov eax, dr7
    and eax, 0xFFFFFF00
    mov dr7, eax
    
    ;*****************************************************
	; Support for soft interrupts on a given thread
	;*****************************************************
	;; this might not be the first time, but a run_thread_int
	mov eax, [ecx + thr_state.sflags]
    mov edx, eax
	and eax, SFLAG_RUN_INT_START
    jz run_thread_int_cont_switch
    and edx, ~SFLAG_RUN_INT_START
    mov dword [ecx + thr_state.sflags], edx          ;; remove the soft int start flag so we won't fall here again
	jmp run_thread_int_cont

run_thread_int_cont_switch:
    ;; Now, since we have implemented software thread switching
	;; by using retf, we will inject on stack0 a virtual call to
	;; callf, as if arch_switch_thread had been called with callf
	;; and retf instead of ret
	;; NOTE: This will only happen first time the thread is runned
	cmp dword [ecx + thr_state.eip], _dummy_eip
	jne _first_time
    
_dummy_eip:
%ifdef FPU_MMX	
	;; if task is NOT mmx_state_owner
	;; set ts, if not, clear it
	cmp ecx, dword [mmx_state_owner]
	jne _mmx_owner
	;; set cr0 TS again
	mov eax, cr0
	or eax, CR0_TS
	mov cr0, eax
	jmp _mmx_cont
_mmx_owner:
	;; clear TS
	clts
_mmx_cont:
%endif
    ;; restore general purpose registers 
	mov ebx, [ecx + thr_state.ebx]
	mov esi, [ecx + thr_state.esi]
	mov edi, [ecx + thr_state.edi]
	
	;; switch to our old stack0
	;; NOTE: When we used soft thread switch
	;; here we restore our ret address
	mov ebp, [ecx + thr_state.ebp]
	mov eax, [ecx + thr_state.esp]
	mov esp, eax
	
    ;; put eflags on eax
	mov eax, [ecx + thr_state.eflags]
    mov edx, [ecx + thr_state.sflags]
	and edx, SFLAG_TRACE_REQ
	jz _dummy_no_trace
    mov edx, [ecx + thr_state.sflags]
    and edx, ~SFLAG_TRACE_REQ
    mov dword [ecx + thr_state.sflags], edx
    or eax, 0x100       ;; set the trap flag, so a step interrupt is executed just when we hit userspace
    ;; our first winding will be created when the debug exception is raised
_dummy_no_trace:
    pop ebp

	;; load eflags register now 
	push eax
	xor eax, eax                        ;; in case we were invoked from run_thread_int		
	popf			;; interrupts should be disabled
	
	ret
	
_first_time:
    ;; set debug registers to 0
    mov eax, 0x400          ;; bit 11 must be 1
    mov dr7, eax
    mov eax, 0xFFFF0FF0       ;; bit 11 must be 1
    mov dr6, eax

	;; load initial mxcsr if SSE is present
	;; only for the first time
%ifdef FPU_MMX
    clts                        ;; clear TS here so ldmxcsr won't raise the exception
    mov eax, [arch_caps]
	and eax, SCAP_SSE
	jz no_sse
	xor eax, eax
	ldmxcsr [arch_caps + 16]
no_sse:	
	;; if task is NOT mmx_state_owner
	;; set ts, if not, clear it
	cmp ecx, dword [mmx_state_owner]
	jne _mmx_cont2
	;; set cr0 TS again
	mov eax, cr0
	or eax, CR0_TS
	mov cr0, eax
_mmx_cont2:	
%endif
    ;; setup our stack0
    xor ebp, ebp
    mov eax, [ecx + thr_state.esp]
	mov esp, eax

	;; simulate inter privilege callf
	;; stack should be like this (grows down):
	;;   _____
	;;  |  ss   | 0
	;;  |  esp  |
	;;  |  cs   |
	;;  |  eip  | <- esp

	push dword [ecx + thr_state.ss]
	push dword [ecx + thr_state.ebp]
	push dword [ecx + thr_state.cs]
	push dword [ecx + thr_state.eip]

	;; set eip to _dummy_eip so we never fall here again
	mov dword [ecx + thr_state.eip], _dummy_eip

	;; put eflags on eax
	mov eax, [ecx + thr_state.eflags]
	
    mov ebx, [ecx + thr_state.sflags]
	and ebx, SFLAG_TRACE_REQ
	jz no_trace
    and ebx, ~SFLAG_TRACE_REQ
    mov dword [ecx + thr_state.sflags], ebx
    or eax, 0x100       ;; set the trap flag, so a step interrupt is executed just when we hit userspace
    ;; our first winding will be created when the debug exception is raised
no_trace:
    ;; first time -> load ds and es
	mov ebx, [ecx + thr_state.es]
	mov es, ebx
	mov ebx, [ecx + thr_state.ds]
	mov ds, ebx

    ;; restore flags (interrupts should be disabled here)
	push eax
    xor eax,eax
    popf

    ;; don't use any flag modifying instructions from here and don't put any instructions here either
    ;; for the trap flag might be set
    
    retf   ;; God help us!!
	

;;***************************************************************************************
;; This function will allow triggering something like an interrupt
;; on a thread. 
;;***************************************************************************************
;; int switch_thread_int(struct thread_state*, int id, unsigned int cr3, unsigned int eip, unsigned int stack);
arch_switch_thread_int:
	mov ecx, [ebp+8]
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_RUN_INT					; check its not on a soft int already
	jz run_thread_int_ok
	mov eax, 1
	ret
run_thread_int_ok:
	or eax, (SFLAG_RUN_INT|SFLAG_RUN_INT_START)
	mov [ecx + thr_state.sflags], eax		; we are now on soft int (starting it)
	jmp arch_switch_thread
	
	;; On next line, thread state has been 
	;; preserved for running thread, but state of 
	;; runned thread has been loaded except
	;; for registers (on C convention), flags and stack0
	;; so we can change everything if we want to
	;; because upon return we will 
	;; recover regs as usual
run_thread_int_cont:
	;; we still have stack0 from the call and we can use ebp
    ;; because it was set by arch_switch_thread
    ;; get stack being used
	cmp dword [ebp + 24], 0x0
	je thread_stack	 
	mov eax, [ebp + 24] 
	jmp run_int_cont
thread_stack:
	mov eax, [ecx + thr_state.esp]
run_int_cont:
	;; load eflags register now 
	mov edx, [ecx + thr_state.eflags]
	push edx
	popf
	;; simulate a far call (return will be performed 
	;; by ret_from_int
	push dword [ecx + thr_state.ss]
	mov eax, esp
	push eax							;; eax contains stack esp
	push dword [ecx + thr_state.cs]		
	push dword [ebp + 20]				;; new eip	
	retf                                ;; Here we need all help we can get, lets pray 
										;; this works :S
			
;; iret/ret would take the task 
;; to it's original eip...
;; this is not good because the original 
;; thread was on sartoris code :(
;; we cannot go back to priv 0 code
;; so we will use ret_from_int syscall
;; which will perform a priv level switch to 0
;; and will lead us here if the soft flag 
;; is set

;;void arch_is_soft_int();
arch_is_soft_int:
	mov ecx, [curr_state]
	mov eax, [ecx + thr_state.sflags]
    mov edx, eax
	and eax, SFLAG_RUN_INT
	jnz arch_is_soft_int_cont
	xor eax, eax
	ret
arch_is_soft_int_cont:
    mov eax, edx
    ;; see if we had a switch on the soft int!
    and eax, SFLAG_RUN_INT_STATE
    jz arch_is_soft_int_cont2
    ;; restore the thread original state to it's state structure
    pop eax
    mov dword [ecx + thr_state.ebp], eax
    pop eax
    mov dword [ecx + thr_state.esi], eax
    pop eax
    mov dword [ecx + thr_state.edi], eax
    pop eax
    mov dword [ecx + thr_state.ebx], eax
    pop eax
    mov dword [ecx + thr_state.esp], eax
arch_is_soft_int_cont2:
    ;; remove softint flags
    and edx, ~(SFLAG_RUN_INT_STATE|SFLAG_RUN_INT|SFLAG_RUN_INT_START)
xchg bx,bx
	;; Now the thread is as if he has just returned
	;; to its original state switch
    ;; ecx points to the "new state"
    ;; and all C registers are safe!
	jmp run_thread_int_cont_switch

;;***************************************************************************************
;; This function will be called when an mmx/fpu instruction is issued
;; It will be called from Device Not Available Exception
;; Returns 1 if no MMX/FPU support, 0 otherwise
;;***************************************************************************************

;; this variable contains 
;; state pointer to the 
;; owner thread of mmx 
;; state
mmx_state_owner:
				dd MMX_NO_OWNER

;; this will run when TS = 1 and someone
;; uses FPU,MMX or SSEn
arch_detected_mmxfpu:
	;; Restore MMX/FPU registers and state
	mov eax, 1	
%ifdef FPU_MMX
	clts				;; clear ts flag so we dont 
						;; generate any more interrupts	
						;; until we switch and someone uses
						;; MMX or FPU	
						;; MUST be done here for save/restore
	;; save last owner state
	mov ecx, [mmx_state_owner]
	mov eax, MMX_NO_OWNER
	cmp ecx, eax
	je no_save

	;; we had an owner, save its 
	;; MMX/FPU state and
	;; change the owner
	
	;; check for fxsave/fxrstor support
	mov eax, [arch_caps]
	and eax, 0x10					;; this is SCAP_FXSR
	jz no_fxsave
	fxsave [ecx + thr_state.mmx]
	jmp mmx_saved
no_fxsave:
	fsave [ecx + thr_state.mmx]
mmx_saved:
	;; set SFLAG_MMXFPU_STORED
	mov eax, [ecx + thr_state.sflags]
	or eax, SFLAG_MMXFPU_STORED
	mov [ecx + thr_state.sflags], eax
no_save:
	;; now to current task...
	mov ecx, [curr_state]
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_MMXFPU_STORED    ;; if this task has an MMX state stored, retore it 
	jz no_load
	;; Check fxrstor support
	mov eax, [arch_caps]
	and eax, 0x10					;; this is SCAP_FXSR
	jz no_fxrstor
	fxrstor [ecx + thr_state.mmx]	
	jmp arch_detected_mmxfpu_cont
no_fxrstor:
	frstor  [ecx + thr_state.mmx]
	jmp arch_detected_mmxfpu_cont
no_load:
	;; Check fxrstor support
	mov eax, [arch_caps]
	and eax, 0x10					;; this is SCAP_FXSR
	jz no_fxrstor_no_load
	fxrstor [mmx_initial_state]
	jmp arch_detected_mmxfpu_cont
no_fxrstor_no_load:
	frstor [mmx_initial_state]
arch_detected_mmxfpu_cont:
	;; set mmx state owner
	mov [mmx_state_owner], ecx	
	xor eax, eax
%endif
	ret


%ifdef FPU_MMX
align   16
mmx_initial_state:
					dw 0x0000, 0x0000, 0x0000, 0x0000
					times 508 dw 0x0000
%endif
