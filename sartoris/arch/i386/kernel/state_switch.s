
bits 32

%define STACK0_SIZE 1416 ;; remember this comes from kernel.h

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

%define SFLAG_MMXFPU_STORED     0x1
%define SFLAG_SSE               0x2       ;; SSE, SSE2 and SSE3
%define SFLAG_RUN_INT           0x4
%define SFLAG_TRACE_REQ         0x20
%define NOT_SFLAG_MMXFPU        0xFFFFFFFE
%define NOT_SFLAG_RUN_INT       0xFFFFFFFB
%define NOT_SFLAG_TRACE_REQ     0xFFFFFFDF
%define CR0_TS                  0x8
%define MMX_NO_OWNER            0xFFFFFFFF

;; Now we use a custom state management structure for threads  
struc thr_state
	.sflags resd 1;
	.eip resd 1;
	.eflags resd 1; 
	.ebx resd 1;
	.ebp resd 1;
	.esp resd 1;
	.esi resd 1;
	.edi resd 1;
	.ss resd 1;
	.fs resd 1;
	.gs resd 1;
	.cs resd 1;
	.es resd 1;
	.ds resd 1;
	.ldt_sel resd 1;
	.stack_winding resd 1;
    .sints resd 1;
%ifdef FPU_MMX
    .padding resd 1;
	.mmx  resd 128;       ;; for preserving FPU/MMX registers
%endif
endstruc

;; size in bytes of thr_state
%ifdef FPU_MMX
%define THR_STATE_SIZE    (576 + STACK0_SIZE) 
%else
%define THR_STATE_SIZE    (60 + STACK0_SIZE) 
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

%ifdef FPU_MMX	
	;; if task is NOT mmx_state_owner
	;; set ts, if not, clear it
	cmp ecx, dword [mmx_state_owner]
	je _mmx_owner
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
    
    ;*****************************************************
	; Support for soft interrupts on a given thread
	;*****************************************************
	;; this might not be the first time, but a run_thread_int
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_RUN_INT
	jnz run_thread_int_cont

run_thread_int_cont_switch:
    ;; Now, since we have implemented software thread switching
	;; by using retf, we will inject on stack0 a virtual call to
	;; callf, as if arch_switch_thread had been called with callf
	;; and retf instead of ret
	;; NOTE: This will only happen first time the thread is runned
	cmp dword [ecx + thr_state.eip], _dummy_eip
	jne _first_time
    
_dummy_eip:
    ;; in case other process is being debugged
    ;; disable debug registers
    xor eax, eax
    mov eax, dr7
    and eax, 0xFFFFFF00
    mov dr7, eax

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
    and edx, NOT_SFLAG_TRACE_REQ
    mov dword [ecx + thr_state.sflags], edx
    or eax, 0x100       ;; set the trap flag, so a step interrupt is executed just when we hit userspace
    ;; our first winding will be created when the debug exception is raised
_dummy_no_trace:

	;; load eflags register now 
	mov eax, [ecx + thr_state.eflags]
	push eax
	popf			;; interrupts should be disabled
	
	pop ebp
	xor eax, eax                        ;; in case we were invoked from run_thread_int		
	ret
	
_first_time:
	;; load initial mxcsr if SSE is present
	;; only for the first time
%ifdef FPU_MMX
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_SSE
	jz no_sse
	xor eax, eax
	ldmxcsr [arch_caps + 16]
no_sse:	
%endif
	;; setup our stack
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
    		
	;; first time -> load ds and es
	mov ebx, [ecx + thr_state.es]
	mov es, ebx
	mov ebx, [ecx + thr_state.ds]
	mov ds, ebx
    
    ;; put eflags on eax
	mov eax, [ecx + thr_state.eflags]

    mov ebx, [ecx + thr_state.sflags]
	and ebx, SFLAG_TRACE_REQ
	jz no_trace
    and ebx, NOT_SFLAG_TRACE_REQ
    mov dword [ecx + thr_state.sflags], ebx
    or eax, 0x100       ;; set the trap flag, so a step interrupt is executed just when we hit userspace
    ;; our first winding will be created when the debug exception is raised
no_trace:
	;; restore flags (interrupts should be disabled here)
	push eax
    xor eax,eax
    popf
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
	or eax, SFLAG_RUN_INT
	mov [ecx + thr_state.sflags], eax		; we are now on soft int
	jmp arch_switch_thread
	
	;; On next line, thread state has been 
	;; preserved for running thread, but state of 
	;; runned thread has been loaded except
	;; for registers (on C convention), flags and stack0
	;; so we can change everything if we want to
	;; because upon return we will 
	;; recover regs as usual
run_thread_int_cont:
	;; we still have stack0 from the call, we can
	;; use ebp
	;; get stack being used
	cmp dword [ebp + 24], 0x0
	je thread_stack	 
	mov eax, [ebp + 24] 
	jmp run_int_cont
thread_stack:
	mov eax, [ecx + thr_state.esp]
run_int_cont:
	;; load eflags register now 
	mov eax, [ecx + thr_state.eflags]
	push eax
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
	and eax, SFLAG_RUN_INT
	jz arch_is_soft_int_cont
	xor eax, eax
	ret
arch_is_soft_int_cont:
	call arch_thread_int_ret
	mov eax, 1
	ret	
arch_thread_int_ret:
	mov ecx, [curr_state]
	;; when the target thread int handler 
	;; made a far call to us, he left on our
	;; stack0 his 4 dwords... remove them!!! muahahahah
	;; when we remove them, we ensure everything is
	;; ok to come back to state switch (because we left 
	;; everything ok except for regs, flags and stack)
	mov eax, esp
	add esp, 16			;; remove far call :)
	
	;; Now the thread is as if he has just returned
	;; to its original state switch
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
