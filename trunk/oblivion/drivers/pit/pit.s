
	;; Oblivion 0.1 PIT support driver
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@cvtci.com.ar      

	

%define PIT_OSCILATOR_FREQ 1193180

%define PIT_CTRL_PORT 0x43
%define PIT_COUNTER_PORT 0x40

%define PIT_CONTROL_WORD 0x36

global adjust_pit

bits 32

section .text

adjust_pit:
	push ebp
	mov ebp, esp
	
	mov eax, PIT_OSCILATOR_FREQ
	xor edx, edx
	div dword [ebp+8]
	
	mov ecx, eax	

	mov al, PIT_CONTROL_WORD
	out PIT_CTRL_PORT, al
	
	mov al, cl
	out PIT_COUNTER_PORT, al
	mov al, ch
	out PIT_COUNTER_PORT, al
	
	pop ebp
	ret

	
