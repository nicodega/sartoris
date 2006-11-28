
;;	Real Time Clock programming routine.
;;
;;	Copyright (C) 2002, 2003, 2004, 2005
;;       
;;	Santiago Bazerque 	sbazerque@gmail.com			
;;	Nicolas de Galarreta	nicodega@gmail.com
;;
;;	
;;	Redistribution and use in source and binary forms, with or without 
;; 	modification, are permitted provided that conditions specified on 
;;	the License file, located at the root project directory are met.
;;
;;	You should have received a copy of the License along with the code,
;;	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
;;	or you can contact us directly at the email addresses provided above.

bits 32

global init_rtc
global read_rtc_status

	;ints per sec:
	;0011    divides xtal by 4 (PC: 8192 ints per second)
        ;0100    divides xtal by 8 (PC: 4096 ints per second)
        ;0101    divides xtal by 16 (PC: 2048 ints per second)
        ;0110    divides xtal by 32 (PC: 1024 ints per second)
        ;0111    divides xtal by 64 (PC: 512 ints per second)
        ;1000    divides xtal by 128 (PC: 256 ints per second)
        ;1001    divides xtal by 256 (PC: 128 ints per second)
        ;1010    divides xtal by 512 (PC: 64 ints per second)
        ;1011    divides xtal by 1024 (PC: 32 ints per second)
        ;1100    divides xtal by 2048 (PC: 16 ints per second)
        ;1101    divides xtal by 4096 (PC: 8 ints per second)
        ;1110    divides xtal by 8192 (PC: 4 ints per second)
        ;1111    divides xtal by 16384 (PC: 2 ints per second)

; void init_rtc(int period)

init_rtc:
	push ebp
	mov ebp, esp
	push ebx

	;; program RTC
	cli
	mov al, 0x0A
	out 0x70, al		; read status A
	in al, 0x71
	mov ebx, [ebp + 8]
	and bl, 0x0F
	and al, 0xF0
	or al, bl		
	out 0x71, al
	mov al, 0x0B
	out 0x70, al		; read status B
	in al, 0x71
	or al, 0x40		; enable periodic interrupts
	out 0x71, al
	sti

	pop ebx
	pop ebp
	ret

read_rtc_status:
	xor eax, eax
	mov al, 0x0C
	out 0x70, al
	in al, 0x71
	ret
