
	;; Sartoris 0.5 i386 PIC management code
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com      
	
bits 32

global reprog_pics
global enable_int_master
global enable_int_slave
global disable_int_master
global disable_int_slave
global eoi_master
global eoi_slave
	
;; PIC ports
PICm0	equ	0x20
PICm1	equ	0x21
PICs0	equ	0xa0
PICs1	equ	0xa1
	
;; PIC Initialization Command Words	
ICW1	equ	0x11		; expect ICW4, cascaded PICs
ICW2_m  equ	0x20		; set int range for master to 0x20-0x27
ICW2_s	equ	0x28		; set int range for slave to 0x28, 0x2f
ICW3_m	equ	0x04		; expect a slave PIC in IRQ2
ICW3_s	equ	0x02		; slave connected to master's IRQ2
ICW4	equ	0x01		; pc-arch, expect software EOI

EOI	equ	0x20 
	
;; PICs
reprog_pics:	
	mov al,	ICW1		; send ICW1 to PICs, 
				; start configuration sequence
	out PICm0, al
	out PICs0, al
	
	mov al, ICW2_m		; master PIC mapped to 0x20-0x27
	out PICm1, al
	mov al, ICW2_s		; slave PIC mapped to 0x28, 0x2f
	out PICs1, al
	
	mov al, ICW3_m		; PIC_0 is master through IRQ2
	out PICm1, al
	mov al, ICW3_s		; PIC_1 is slave through IRQ2
	out PICs1, al
	
	mov al, ICW4		; pc-env, soft-int-ack for both
	out PICm1, al
	out PICs1, al

	mov al, 0xfb		; disable all ints from master, but int2 
				; (slave pic)
	out PICm1, al

	mov al, 0xff		; disable all ints from slave
	out PICs1, al
	
	ret

enable_int_master:
	push ebp
	mov ebp, esp
	
	in al, PICm1
	mov ecx, [ebp+8]
	mov edx, 1
	shl edx, cl
	not edx
	and eax, edx
	out PICm1, al

	pop ebp
	ret

enable_int_slave:
	push ebp
	mov ebp, esp
	
	in al, PICs1
	mov ecx, [ebp+8]
	mov edx, 1
	shl edx, cl
	not edx
	and eax, edx
	out PICs1, al

	pop ebp
	ret

disable_int_master:
	push ebp
	mov ebp, esp
	
	in al, PICm1
	mov ecx, [ebp+8]
	mov edx, 1
	shl edx, cl
	or eax, edx
	out PICm1, al

	pop ebp
	ret
	
disable_int_slave:	
	push ebp
	mov ebp, esp
	
	in al, PICs1
	mov ecx, [ebp+8]
	mov edx, 1
	shl edx, cl
	or eax, edx
	out PICs1, al

	pop ebp
	ret
	



