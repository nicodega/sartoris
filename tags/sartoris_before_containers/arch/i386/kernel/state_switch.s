
%define STACK0_SIZE 1024
%define PAGING

global arch_switch_thread
global arch_detected_mmxfpu

%ifdef _SOFTINT_
global arch_switch_thread_int
global arch_thread_int_ret
%endif

extern thr_states
extern global_tss
extern stacks
extern curr_state

extern arch_caps

;;
;; In order not to perform an mmx/fpu preservation always
;; because its costs us much, we will do as intel tells us.
;; We will set TS bit 3 on cr0 to 0. When we are switching
;; we will check the value of the register, and if it's non
;; 0, we will preserve. Otherwise we won't.
;;

%define SFLAG_MMXFPU            0x1
%define SFLAG_MMXFPU_STORED     0x2
%define SFLAG_RUN_INT           0x4
%define SFLAG_SSE               0x10       ;; SSE, SSE2 and SSE3
%define NOT_SFLAG_MMXFPU        0xFFFFFFFE
%define NOT_SFLAG_RUN_INT       0xFFFFFFEF
%define CR0_TS                  0x8

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
%ifdef FPU_MMX
	.mmx  resd 128;       ;; for preserving FPU/MMX registers
%endif
endstruc

;; size in bytes of thr_state
%define THR_STATE_SIZE    572  

;;
;; Software based task switching support.
;;
;; void arch_switch_thread(int thr_id, unsigned int cr3)
arch_switch_thread:

	;; ebp, ebx, esi, and edi must be preserved
	push ebp
	mov ebp, esp
		
	;; Preserve current thread state

	;; ds:ecx will contain thread state 
	;; mov eax,[ecx + thr_state.xxxx]
	mov ecx, [curr_state]					;; now ecx contains &thr_states[thread last switched]
	
	;; edx will contain cr3 for new thread
	mov edx, [ebp+12]			            ;; now edx contains cr3 for new thread
	
	;; preserve segment selectors (es and ds wont be saved, for they are loaded with kernel selectors)
	;; ss wont be preserved, because it was already preserved on far call to interrupt or run thread
	;; anyway, we don't need it, because we are on stack0
	mov eax, fs
	mov [ecx + thr_state.fs], eax
	mov eax, gs
	mov [ecx + thr_state.gs], eax
	
	;; preserve MMX/FPU
%ifdef FPU_MMX
	;; did thread use MMX/FPU?
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_MMXFPU
	jz _no_mmx_fpu_used	
	
	;; check for fxsave/fxrstor support
	mov eax, [arch_caps]
	and eax, 0x10					;; this is SCAP_FXSR
	jz no_fxsave
	fxsave [ecx + thr_state.mmx]
	jmp mmx_save_cont
no_fxsave
	fsave [ecx + thr_state.mmx]
	;; clear the flag so nex time we wont preserve MMX/FPU
	;; unless the thread uses them
mmx_save_cont:
	and dword [ecx + thr_state.sflags], NOT_SFLAG_MMXFPU
	
_no_mmx_fpu_used:
	;; set cr0 TS again
	mov eax, cr0
	or eax, CR0_TS
	mov cr0, eax	
%endif

%ifndef DONT_LDT_SWITCH
	;; preserve ldt selector register
	sldt [ecx + thr_state.ldt_sel]
%endif

	;; preserve flags
	pushf
	pop eax
	mov [ecx + thr_state.eflags], eax
	
	;; preserve stack
	mov eax, esp
	mov [ecx + thr_state.esp], eax
	
	;; preserve general purpose registers now (I'll only preserve C registers here)
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
	mov ecx, global_tss
	add ecx, 4
	mov eax, STACK0_SIZE
	mul dword [ebp+8]		   ;; we can still tho this, because we did not recover the stack yet.
	add eax, (stacks+STACK0_SIZE-4)
	mov [ecx], eax             ;; global_tss.esp0 = (unsigned int)(&stacks[task_id][0]) + STACK0_SIZE - 4;
	
	;; ds:ecx will contain thread state 
	mov eax, THR_STATE_SIZE
	mul dword [ebp+8]           
	add eax, thr_states
	mov ecx, eax               ;; now ecx contains &thr_states[id]
	mov [curr_state], ecx      ;; update curr_state to the new one
	
	;; restore ldt (no problem again, since we are on kernel segments)
%ifndef DONT_LDT_SWITCH
	lldt [ecx + thr_state.ldt_sel]
%endif		
	
	;; restore segments (no problem again, since we are on kernel segments)
	;; ss wont be restored, because switch happens on stack0
	mov eax, [ecx + thr_state.fs]
	mov fs, eax
	mov eax, [ecx + thr_state.gs]
	mov gs, eax
		
    ;; Now, since we have implemented software thread switching
	;; by using retf, we will inject on stack0 a virtual call to
	;; callf, as if arch_switch_thread had been called with callf
	;; and retf instead of ret
	;; NOTE: This will only happen first time the thread is runned
	cmp dword [ecx + thr_state.eip], _dummy_eip
	jne _first_time
_dummy_eip:

%ifdef _SOFTINT_
	;*****************************************************
	; Support for fake interrupts on the same thread
	;*****************************************************
	;; this might not be the first time, but a run_thread_int
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_RUN_INT
	jnz run_thread_int_cont
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
		
	;; load eflags register now 
	mov eax, [ecx + thr_state.eflags]
	push eax
	popf
	
	pop ebp
	xor eax, eax                        ;; in case we were invoked from run_thread_int		
	ret
	
	
_first_time:
	;; load initial mxcsr if SSE is present
	;; only for the first time
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_SSE
	jz no_sse
	xor eax, eax
	ldmxcsr [arch_caps + 16]
no_sse:	
	;; setup our stack0
	xor ebp, ebp
	mov eax, [ecx + thr_state.esp]
	mov esp, eax
		
	;; load eflags register now 
	mov eax, [ecx + thr_state.eflags]
	push eax
	popf
	
	;; simulate inter privilege callf
	;; stack should be like this (grows down):
	;;   _____
	;;  |  ss   | 0
	;;  |  esp  |
	;;  |  cs   |
	;;  |  eip  |
	;;  |       | <- esp
	push dword [ecx + thr_state.ss]
	push dword [ecx + thr_state.ebp]
	push dword [ecx + thr_state.cs]
	push dword [ecx + thr_state.eip]
	
	;; set eip to _dummy_eip so we never fall here again
	mov dword [ecx + thr_state.eip], _dummy_eip

	;; first time -> load ds and es
	mov eax, [ecx + thr_state.es]
	mov es, eax
	mov eax, [ecx + thr_state.ds]
	mov ds, eax
	
	retf   ;; God help us!!
			
	
%ifdef _SOFTINT_
;;***************************************************************************************
;; This function will allow triggering something like an interrupt
;; on a thread. 
;;***************************************************************************************
;; int switch_thread_int(int id, unsigned int cr3, unsigned int eip, unsigned int stack);
arch_switch_thread_int:
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_RUN_INT
	jz run_thread_int_ok
	mov eax, 1
	ret
run_thread_int_ok:
	or eax, SFLAG_RUN_INT
	mov [ecx + thr_state.sflags], eax
	jmp arch_switch_thread
	
	;; On next line, thread state has been 
	;; preserved for running thread, but state of 
	;; runned thread has been loaded except
	;; for registers, flags and stack0
	;; so we can change everything if we want to
	;; because upon return we will 
	;; recover regs as usual
run_thread_int_cont:
	;; we still have stack0 from the call, we can
	;; use ebp
	;; get stack being used
	cmp dword [ebp + 20], 0x0
	je thread_stack	 
	mov eax, [ebp + 20] 
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
	push dword [ebp + 16]				;; new eip	
	retf                                ;; Here we need all help we can get, lets pray 
										;; this works :S
	
		
;; ret from int will take the task 
;; to it's original eip...
;; this is not good because the original 
;; thread was on sartoris code :(
;; we cannot go back to priv 0 code
;; so we create will use ret_from_int syscall
;; which will perform a priv level switch to 0
;; and will lead us here if the soft flag 
;; is set

;; return from a fake interrupt
; void arch_thread_int_ret()
;; ret from int should leave curr_state on ecx... 
;; but lets load it anyway
arch_thread_int_ret:
	mov ecx, [curr_state]
	;; when the target thread int handler 
	;; made a far call to us, he left on our
	;; stack 0 his 4 dwords... remove them!!! muahahahah
	;; when we remove them, we ensure everything is
	;; ok to come back to state switch (because we left 
	;; everything ok except for regs, flags and stack)
	;; We must leave registers like this:
	;; 
	mov eax, esp
	add esp, 16			;; remove far call :)
	
	;; Now the thread is as if he has just returned
	;; to its original state switch
	jmp _dummy_eip      ;; at last! dummy_eip IS useful!
%endif
		

;;***************************************************************************************
;; This function will be called when an mmx/fpu instruction is issued
;; It will be called from Device Not Available Exception
;; Returns 1 if no MMX/FPU support, 0 otherwise
;;***************************************************************************************

arch_detected_mmxfpu:

	;; ok, current task used the FPU, set the 
	;; sflag, so we preserve it on next switch
	;; and restore MMX/FPU registers and state

	mov eax, 1
%ifdef FPU_MMX
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_MMXFPU_STORED 
	jz arch_detected_mmxfpu_never_used
	;; Check fxrstor support
	mov eax, [arch_caps]
	and eax, 0x10					;; this is SCAP_FXSR
	jz no_fxrstor
	fxrstor [ecx + thr_state.mmx]	
	jmp arch_detected_mmxfpu_cont
no_fxrstor:
	frstor  [ecx + thr_state.mmx]
arch_detected_mmxfpu_cont:
	or dword [ecx + thr_state.sflags], SFLAG_MMXFPU
	clts				;; clear ts flag so we dont 
						;; generate any more interrupts	
						;; until we switch and someone uses
						;; MMX or FPU
	xor eax, eax
%endif
	ret
%ifdef FPU_MMX
arch_detected_mmxfpu_never_used:
	or dword [ecx + thr_state.sflags], SFLAG_MMXFPU_STORED        ;; next time we will load FPU/MMX/SSE state 
	xor eax, eax
	ret	
%endif



