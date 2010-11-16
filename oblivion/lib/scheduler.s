bits 32

%include "../include/sartoris-i386/gdt-syscalls.h"

global reschedule

reschedule:
	push ebp
	mov ebp, esp
	push dword 1			; SCHED_THR
	call RUN_THREAD : 0x00000000	; RUN_THREAD SYSCALL
	pop ebp
	ret
