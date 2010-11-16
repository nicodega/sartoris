%define PICm0 0x20		; the master pic port 0
%define PICm1 0x21		; the master pic port 1

bits 32

global killsched	

killsched:	
	in al, PICm1 		; read current mask (for master)
	
	or al, 0x01		; disable the scheduler  
				;(IRQ 0, master)
	out PICm1, al

	ret
