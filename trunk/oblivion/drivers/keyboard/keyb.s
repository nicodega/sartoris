
	;; Oblivion 0.1 keyboard driver
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@cvtci.com.ar      

	

	;; this thread will handle IRQ1, the keyboard.

	;; the keyboard has a very primitive interface, and will interrupt us
	;; more than once per keystroke. we will use cpu registers (that are
	;; saved/restored with the task switch) to save the ctrl-alt-shift map
	;; and the old interrupt mask in order to make this as light as 
	;; possible.

	;; the keys are stored in a FIFO queue. by default this driver
	;; stores a ctrl-alt byte followed by an ascii code (the array length
	;; must be even). if simple_mode is %defined, only the ascii byte is
	;; saved.
	
	;; initialization is performed in 
	;;	listening_thread_entry:
	
	;; afterwards, the thread enters a perpetual loop reading input 
	;; from the keyboard.

	
	;; the register layout during the read routine:

	;; cl -> saved interrupt mask
	;; bh -> EXT flag
	;; bl -> shift-alt-ctrl map

	;; bh usage:	bit 0 -> EXT
	
	;; bl usage:	bit 0 -> left	shift
	;;		bit 1 -> right	shift
	;;		bit 2 -> left	ctrl
	;;		bit 3 -> right	ctrl
	;;		bit 4 -> left	alt
	;;		bit 5 -> right	alt

	;; the readed keys are stored in the array kbd_buf. The int variables
	;; next_read and next_write indicate the array position to be readed
	;; next, and the first already-readed position. if next_read=next_write,
	;; the FIFO queue may be full, or empty. a byte variable full_buf 
	;; distinguishes these cases.

	;; 19/8/2002
	;;
	;; I disabled nesting_mode because it was hanging the driver
	;; under the bochs simulator. The problem appears when
	;; a timer interrupt occurs quickly after a keyboard one.
	;; Then, the PIC signals the second interrupt (the one
	;; from the timer) to the processor before this handler
	;; has time to change the PIC mask. Therefore, as soon
	;; as we sti, the processor fires the pending interrupt.
	;; This problem never appears with real hardware (so far).
	;; Probably it's because the vulnerable window is just
	;; too narrow, or maybe it's because the real PIC ignores
	;; interrupt lines rised while it is negotiating with the
	;; processor. Since nesting_mode doesn't make any
	;; noticeable difference with this driver (the handler is
	;; quite simple and fast), I decided to disable it.
	;; I am researching the problem. If I am sure that nesting
	;; will not crash the driver outside the simulator,
	;; I will put it back.
	;;
	;;	Santiago

		
bits 32
	
;; %define simple_mode
;; %define nesting_mode		

%include "../../include/sartoris-i386/gdt-syscalls.h"
	
%define PICm0 0x20		; the master pic port 0
%define PICm1 0x21		; the master pic port 1
	
%define EOI 0x20		; end of interrupt code
	
%define KBD_PORT 0x60
%define KBD_ACK_PORT 0x61
	
%define KBD_BUF_LEN 256
	
	
%define KEY_UP_CODE 0xf0
%define KEY_EXT_CODE 0xe0

%define LSHIFT_CODE	0x2a	
%define RSHIFT_CODE	0x36	
%define CTRL_CODE	0x1d
%define ALT_CODE	0x38
	
%define LSHIFT 0x01
%define RSHIFT 0x02
%define LCTRL  0x04
%define RCTRL  0x08
%define LALT   0x10
%define RALT   0x20

%define KEY_EXT	1

%define F 0x80
%define NK 0xff
%define CK 0xfe
%define tab 0x09
%define tilde 0x7e
%define esc 0x1b
%define bs 0x08
%define nl 0x0a
%define UP 0x90
%define DOWN 0x91
%define LEFT 0x92
%define RIGHT 0x93

extern next_read
extern next_write
extern kbd_buf
extern full_buf

global keyb_int_thread_entry
global read_code		; not really global, but i want to breakpoint
		
section .text

keyb_int_thread_entry:
	
	xor eax, eax		; the initial TSS has this registers in zero,
	xor ebx, ebx		; so this shouldn't be really necessary.
	xor ecx, ecx		; it's harmless, so we may as well do it.

	mov esi, kbd_buf	; address of the array used for FIFO output.
	
	;; we have spare registers!
		
read_code:	
	
%ifdef nesting_mode	
	in al, PICm1		; read current mask (for master)
	mov [int_mask], al	; save it
	
	or al, 0x03		; disable the scheduler and keyb 
				;(IRQ 0-1, master)
	out PICm1, al		 

	sti			; ok, interrupts are back up.		

%endif			

	mov al, EOI		; first things first
	out PICm0, al		; acknowledge the interrupt


	in al, KBD_PORT		; read keycode
	mov edx, eax		; save it

	in al, KBD_ACK_PORT	; send acknowledge to the keyboard 
	mov ah, al
	or al, 0x80		; disable keyboard
	out KBD_ACK_PORT, al
	mov al, ah
	out KBD_ACK_PORT, al	; re-enable keyboard

	mov eax, edx		; restore keycode

		
	cmp al, KEY_EXT_CODE	; if this is just an EXT code, 
				; set bh and wait (do nothing)
	jne not_ext
	or bh, KEY_EXT
	jmp done
not_ext:			; ok, no EXT. control key, i presume?
	and dl, 0x7f
	cmp byte [key2asc+edx], CK
	jnz skip_update		
	call update_control_keys
	and bh, ~(KEY_EXT)
	jmp done
	
skip_update:			; no control key
	test al, 0x80		; if this is the release of a key,
	jz key_press		; do nothing
	and bh, ~(KEY_EXT)	; we received the ext key already,
				; so clear the flag
	jmp done

key_press:			; ok, so the keyboard is sending an
				; actual keycode
			
	test bl, (RSHIFT | LSHIFT)
	jz no_shift
	mov edi, shift_key2asc
	jmp done_mode
no_shift:	
	mov edi, key2asc	; this shouldn't be necessary!
done_mode:
	mov dl, [edi+eax]
	cmp dl, NK
	je done
	
	cmp byte [full_buf], 0	; if our buffer is full, do nothing
	je go			;
	and bh, ~(KEY_EXT) 
	jmp done
go:		
	
	mov edi, [next_write]	; read next write position

%ifndef simple_mode
	mov [esi+edi], bl
	inc edi
%endif
	mov [esi+edi], dl
	inc edi
	cmp edi, KBD_BUF_LEN
	jne still_room
	xor edi, edi
still_room:
	mov [next_write], edi
	cmp edi, [next_read]
	jne not_full
	mov byte [full_buf], 0xff
not_full:
	and bh, ~(KEY_EXT)
done:


%ifdef nesting_mode
	cli			; disable interrupts
	
	mov al, [int_mask]	; restore interrupt mask
	out PICm1, al		; 
%endif
	pusha		
	call RET_FROM_INT : 0x00000000
	popa
	jmp read_code		; ok, we're back. let's do it again!



	
	;; this routine keeps track of the state of the ctrl, alt & shift keys.
	;; it would make sense to write it in C, but assembly leaves more
	;; room for useless comments ;)

	;; you know, i like to express myself during these lonely nights.
	
update_control_keys:
	mov edx, eax
	and al, 0x7f
	
	cmp al, LSHIFT_CODE	; left shift has changed
	jne no_lshift
	test dl, 0x80
	jz lshift_press
	and bl, ~(LSHIFT)
	ret
lshift_press:
	or bl, LSHIFT
	ret

no_lshift:	
	cmp al, RSHIFT_CODE	; right shift has changed
	jne no_rshift
	test dl, 0x80
	jz rshift_press
	and bl, ~(RSHIFT)
	ret
rshift_press:
	or bl, RSHIFT
	ret

no_rshift:
	cmp al, CTRL_CODE	; ctrl, maybe?
	jne no_control
	test bh, KEY_EXT
	jz lctrl
	test dl, 0x80
	jz rctrl_press
	and bl, ~(RCTRL)
	and bh, ~(KEY_EXT)
	ret
rctrl_press:	
	or bl, RCTRL
	and bh, ~(KEY_EXT)
	ret

lctrl:
	test dl, 0x80
	jz lctrl_press
	and bl, ~(LCTRL)
	ret
lctrl_press:	
	or bl, LCTRL
	ret
	
no_control:
	cmp al, ALT_CODE	; alt changed
	jne no_alt
	test bh, KEY_EXT
	jz lalt
	test dl, 0x80
	jz ralt_press
	and bl, ~(RALT)
	and bh, ~(KEY_EXT)
	ret
ralt_press
	or bl, RALT
	and bh, ~(KEY_EXT)
	ret
lalt:
	test dl, 0x80
	jz lalt_press
	and bl, ~(LALT)
	ret
lalt_press:
	or bl, LALT
	ret

no_alt:				; we should never get here, if the keytable was
	ret			; checked and a control key was found



int_mask: db 0xff	
	
	;; these are the keymaps. they should work reasonably for english 
	;; at-keyboards.
	
key2asc:

db NK, esc, '1', '2', '3', '4', '5', '6'		; 0x00-0x07
db '7', '8', '9', '0', '-', '=', bs, tab		; 0x08-0x0f
db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'		; 0x10-0x17
db 'o', 'p', '[', ']', nl, CK, 'a', 's'			; 0x18-0x1f
db 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'		; 0x20-0x27  undef: 0x27
db 0x40, NK, CK, NK, 'z', 'x', 'c', 'v'			; 0x28-0x2f  el 0x28 (0x40) es "'", undef: 0x29 
db 'b', 'n', 'm', ',', '.', '/', CK, '*'		; 0x30-0x37
db CK, ' ', CK, F+1, F+2, F+3, F+4, F+5			; 0x38-0x3f
db F+6, F+7, F+8, F+9, F+10, CK, CK, '7'		; 0x40-0x47
db UP, '9', '-', LEFT, '5', RIGHT, '+', '1'		; 0x48-0x4f
db DOWN, '3', '0', '.', NK, NK, NK, F+11			; 0x50-0x57 
db F+12
times 7 db NK

shift_key2asc:
db NK, esc, '!', '@', '#', '$', '%', '^'		; 0x00-0x07
db '&', '*', '(', ')', '_', '+', bs, tab		; 0x08-0x0f
db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'		; 0x10-0x17
db 'O', 'P', '{', '}', nl, CK, 'A', 'S'			; 0x18-0x1f
db 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':'		; 0x20-0x27  undef: 0x27
db 0x40, NK, CK, NK, 'Z', 'X', 'C', 'V'			; 0x28-0x2f  el 0x28 (0x40) es "'", undef: 0x29 
db 'B', 'N', 'M', '<', '>', '?', CK, '*'		; 0x30-0x37
db CK, ' ', CK, F+1, F+2, F+3, F+4, F+5			; 0x38-0x3f
db F+6, F+7, F+8, F+9, F+10, CK, CK, '7'		; 0x40-0x47
db UP, '9', '-', LEFT, '5', RIGHT, '+', '1'		; 0x48-0x4f
db DOWN, '3', '0', '.', NK, NK, NK, F+11		; 0x50-0x57 
db F+12
times 7 db NK
