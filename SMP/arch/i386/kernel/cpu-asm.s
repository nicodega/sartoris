
	;; Sartoris i386 low level functions that didn't fit elsewhere
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com      
	
	
bits 32

extern k_scr_print
extern k_scr_hex
global arch_dump_cpu
global get_flags

get_flags:
	pushf
	pop eax
	ret

arch_dump_cpu:

;-----------
pFLAGS equ 52
;-----------
pCS    equ 48
;-----------
pEIP   equ 44
;-----------
pERR  equ 40
;-----------
pEAX    equ 36
pECX    equ 32
pEDX    equ 28
pEBX    equ 24
pEBP    equ 20
pESI    equ 16
pEDI    equ 12  
pDS     equ 8
pES     equ 4  
;; <- esp

	mov ebp, esp

	push dword 0x7   ; text color for k_scr_print/k_scr_hex
		
	;; EAX
	push dword r_eax
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pEAX]
	push eax
	call k_scr_hex
	add esp, 4

	;; EBX
	push dword r_ebx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pEBX]
	push eax
	call k_scr_hex
	add esp, 4

	;; ECX
	push dword r_ecx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pECX]
	push eax
	call k_scr_hex
	add esp, 4

	;; EDX
	push dword r_edx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pEDX]
	push eax
	call k_scr_hex
	add esp, 4
	
	;; ESI
	push dword r_esi
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+pESI]
	push eax
	call k_scr_hex	
	add esp, 4

	;; EDI
	push dword r_edi
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pEDI]
	push eax
	call k_scr_hex
	add esp, 4

	;; EBP
	push dword r_ebp
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+pEBP]
	push eax
	call k_scr_hex
	add esp, 4
	
	;; ESP -- this one I believe is still wrong
	push dword r_esp
	call k_scr_print
	add esp, 4
	
	mov eax, esp
	sub eax, 52
	push eax
	call k_scr_hex
	add esp, 4

    ;; CS
	push dword r_cs
	call k_scr_print
	add esp, 4
		
	mov eax, [ebp+pCS]
	push eax
	call k_scr_hex
	add esp, 4

	;; DS
	push dword r_ds
	call k_scr_print
	add esp, 4
		
	mov eax, [ebp+pDS]
	push eax
	call k_scr_hex
	add esp, 4

	;; ES
	push dword r_es
	call k_scr_print
	add esp, 4
		
	mov eax, [ebp+pES]
	push eax
	call k_scr_hex
	add esp, 4

	;; SS
	push dword r_ss
	call k_scr_print
	add esp, 4

	xor eax, eax
	mov ax, ss
	push eax
	call k_scr_hex
	add esp, 4
	
	;; FS
	push dword r_fs
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, fs
	push eax
	call k_scr_hex
	add esp, 4

	;; GS
	push dword r_gs
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, gs
	push eax
	call k_scr_hex
	add esp, 4
	
	;; FLAGS
	push dword r_flags
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+pFLAGS]
	push eax
	call k_scr_hex
	
	;; ERROR	
	push dword r_err
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+pERR]
	push eax
	call k_scr_hex
	add esp, 4
	
	;; EIP
	push dword r_eip
	call k_scr_print
	add esp, 4

	mov eax, [ebp+pEIP]
	push eax
	call k_scr_hex
			
	add esp, 4	
	ret
	

r_eax:	db 0xa, "    eax = 0x", 0
r_ebx:	db ", ebx = 0x", 0
r_ecx:	db ", ecx = 0x", 0
r_edx:	db ", edx = 0x", 0	
r_esi:	db 0xa, "    esi = 0x", 0
r_edi:	db ", edi = 0x", 0
r_ebp:	db ", ebp = 0x", 0
r_esp:	db ", esp = 0x", 0
r_cs:	db 0xa, "    cs  = 0x", 0
r_ds:	db ", ds  = 0x", 0
r_es:	db ", es  = 0x", 0
r_ss:	db 0xa, "    ss  = 0x", 0
r_fs:	db ", fs  = 0x", 0
r_gs:	db ", gs  = 0x", 0
r_flags: db 0xa, "    eflags = 0x", 0
r_err: db 0xa, "    error = 0x", 0
r_eip: db ", eip = 0x", 0
	
