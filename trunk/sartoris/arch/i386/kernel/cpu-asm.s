
	;; Sartoris i386 low level functions that didn't fit elsewhere
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@cvtci.com.ar      
	
	
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

	push esp
	push ebp
	push edi
	push esi
	push edx
	push ecx
	push ebx
	push eax
	pushfd
	mov ebp, esp

	push dword 0x7
	
	push dword r_eax
	call k_scr_print
	add esp, 4

	mov eax, [ebp+4]
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_ebx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+8]
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_ecx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+12]
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_edx
	call k_scr_print
	add esp, 4

	mov eax, [ebp+16]
	push eax
	call k_scr_hex
	add esp, 4
	
	push dword r_esi
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+20]
	push eax
	call k_scr_hex	
	add esp, 4

	push dword r_edi
	call k_scr_print
	add esp, 4

	mov eax, [ebp+24]
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_ebp
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+28]
	push eax
	call k_scr_hex
	add esp, 4
	
	push dword r_esp
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp+32]
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_cs
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, cs
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_ds
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, ds
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_es
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, es
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_ss
	call k_scr_print
	add esp, 4

	xor eax, eax
	mov ax, ss
	push eax
	call k_scr_hex
	add esp, 4
	
	push dword r_fs
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, fs
	push eax
	call k_scr_hex
	add esp, 4

	push dword r_gs
	call k_scr_print
	add esp, 4
		
	xor eax, eax
	mov ax, gs
	push eax
	call k_scr_hex
	add esp, 4
	
	push dword r_flags
	call k_scr_print
	add esp, 4
	
	mov eax, [ebp]
	push eax
	call k_scr_hex
	add esp, (4+40)
	ret
die:
	jmp die
	

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
	
