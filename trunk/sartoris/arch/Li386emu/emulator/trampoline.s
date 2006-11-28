bits 32

%define ACT_CLOCK 0
%define ACT_SYSCALL 1
%define ACT_IO 2
	
global sigtimer_handler
global syscall_handler
global emu_action
global entry_esp
global dispatch_from_scratch
global dispatch_thread
extern emu_trampoline
	
section .text

io_handler:
	mov dword [emu_action], ACT_IO
	jmp enter_emu
	
syscall_handler:
	mov dword [emu_action], ACT_SYSCALL
	jmp enter_emu
		
sigtimer_handler:
	mov dword [emu_action], ACT_CLOCK
enter_emu:		
	mov dword [entry_esp], esp
	
	call emu_trampoline

	ret
	
dispatch_from_scratch:
	push dword [reg_eflags]
	popf
	mov eax, [reg_eax]
	mov ebx, [reg_ebx]
	mov ecx, [reg_ecx]
	mov edx, [reg_edx]
	mov esi, [reg_esi]
	mov edi, [reg_edi]
	mov ebp, [reg_ebp]
	mov esp, [reg_esp]
	jmp [reg_eip]
	
section .data
		
emu_action: dd 0
	
entry_esp: dd 0

dispatch_thread:
	
reg_eax:	dd 0
reg_ebx:	dd 0
reg_ecx:	dd 0
reg_edx:	dd 0
reg_esi:	dd 0
reg_edi:	dd 0
reg_ebp:	dd 0
reg_esp:	dd 0
reg_eip:	dd 0
reg_eflags:	dd 0
