bits 32

%define PICm0 0x20		; the master pic port 0
%define EOI 0x20
	
	;; until we have mutex in the process manager, i need a way to obtain mutual exclusivity

global enter_block
global exit_block
global ack_int_master
	
enter_block:
	cli
	ret

exit_block:
	sti
	ret

ack_int_master:		
	mov al, EOI		; first things first
	out PICm0, al		; acknowledge the interrupt
	ret
