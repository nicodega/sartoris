
%ifdef FPU_MMX
%define STACK0_SIZE 1407 ;; remember this comes from kernel.h
%else
%define STACK0_SIZE 1991
%endif

%define SCAP_SSE                0x20       ;; SSE, SSE2 and SSE3
%define SFLAG_MMXFPU_STORED     0x1
%define SFLAG_RUN_INT           0x2
%define SFLAG_RUN_INT_START     0x4
%define SFLAG_RUN_INT_STATE     0x8
%define SFLAG_TRACE_REQ         0x40
%define CR0_TS                  0x8
%define MMX_NO_OWNER            0xFFFFFFFF

%define SFLAG_TRACEABLE         0x10
%define SFLAG_DBG               0x20
%define SFLAG_TRACE_END         0x80
%define KRN_DATA                0x10
%define TRAP_FLAG               0x100

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
%define THR_STATE_SIZE    (584 + STACK0_SIZE) 
%else
%define THR_STATE_SIZE    (68 + STACK0_SIZE) 
%endif
