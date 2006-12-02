
%define STACK0_SIZE 1024
%define PAGING

global arch_switch_thread
global arch_detected_mmxfpu

extern thr_states
extern global_tss
extern stacks
extern curr_thread

;;
;; In order not to perform an mmx/fpu preservation always
;; because its costs us much, we will do as intel tells us.
;; We will set TS bit 3 on cr0 to 0. When we are switching
;; we will check the value of the register, and if it's non
;; 0, we will preserve. Otherwise we won't.
;;

%define SFLAG_MMXFPU       0x1
%define NOT_SFLAG_MMXFPU   0xFFFFFFFE
%define CR0_TS             0x8

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

	;; I want bochs to stop here ;;
	xchg bx,bx
	
	;; ds:ecx will contain thread state 
	;; mov eax,[ecx + thr_state.xxxx]
	mov eax, THR_STATE_SIZE
	mul dword [curr_thread]			
	add eax, thr_states
	mov ecx, eax					;; now ecx contains &thr_states[curr_thr]
	
	;; edx will contain cr3 for new thread
	mov edx, [ebp+12]			;; now edx contains cr3 for new thread
	
	;; preserve segment selectors (es and ds wont be saved, for they are loaded with kernel selectors)
	mov [ecx + thr_state.ds], eax
	mov eax, fs
	mov [ecx + thr_state.fs], eax
	mov eax, gs
	mov [ecx + thr_state.gs], eax
	mov eax, ss
	mov [ecx + thr_state.ss], eax
	
	;; preserve MMX/FPU
%ifdef FPU_MMX
	;; did thread use MMX/FPU?
	mov eax, [ecx + thr_state.sflags]
	and eax, SFLAG_MMXFPU
	jz _no_mmx_fpu_used	
	
	fxsave [ecx + thr_state.mmx]
	
	;; clear the flag so nex time we wont preserve MMX/FPU
	;; unless the thread uses them
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

	;; restore ldt (no problem again, since we are on kernel segments)
%ifndef DONT_LDT_SWITCH
	lldt [ecx + thr_state.ldt_sel]
%endif		
	
	;; restore segments (no problem again, since we are on kernel segments)
	mov [ecx + thr_state.fs], eax
	mov fs, eax
	mov [ecx + thr_state.gs], eax
	mov gs, eax
	mov [ecx + thr_state.ss], eax
	mov ss, eax
	
	;; restore general purpose registers 
	mov ebx, [ecx + thr_state.ebx]
	mov esi, [ecx + thr_state.esi]
	mov edi, [ecx + thr_state.edi]
	
	;; load eflags register now end return happily
	mov eax, [ecx + thr_state.eflags]
	push eax
	popf

	;; switch to our old stack0
	mov ebp, [ecx + thr_state.ebp]
	mov eax, [ecx + thr_state.esp]
	mov esp, eax	

;; Now, since we have implemented software thread switching
	;; by using retf, we will inject on stack0 a virtual call to
	;; callf, as if arch_switch_thread had been called with callf
	;; and retf instead of ret
	;; NOTE: This will only happen once
	cmp dword [ecx + thr_state.eip], _dummy_eip
	jne _first_time
_dummy_eip:
	
	pop ebp
	ret
_first_time:
	;; set eip to _dummy_eip so we never fall here again
	mov dword [ecx + thr_state.eip], _dummy_eip
	
	;; simulate inter privilege callf
	;; stack should be like this (grows down):
	;;   _____
	;;  |  ss   | 0
	;;  |  esp  |
	;;  |  cs   |
	;;  |  eip  |
	;;  |       | <- esp
	push dword [ecx + thr_state.ss]
	push dword 0
	push dword [ecx + thr_state.cs]
	push dword [ecx + thr_state.eip]
	
	retf   ;; God help us!!



	

;; This function will be called when an mmx/fpu instruction is issued
;; It will be called from Device Not Available Exception
;; Returns 1 if no MMX/FPU support, 0 otherwise

arch_detected_mmxfpu:

	;; ok, current task used the FPU, set the 
	;; sflag, so we preserve it on next switch
	;; and restore MMX/FPU registers and state

	mov eax, 1
%ifdef FPU_MMX
	fxrstor [ecx + thr_state.mmx]	
	or dword [ecx + thr_state.sflags], SFLAG_MMXFPU
	clts				;; clear ts flag so we dont 
						;; generate any more interrupts	
						;; until we switch and someone uses
						;; MMX or FPU
	
%endif
	xor eax, eax
	ret
