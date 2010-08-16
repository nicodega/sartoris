    
;; Oblivion 0.1 keyboard driver
;;
;; Copyright (C) 2002, 2003, 2004, 2005
;;       
;; Santiago Bazerque     sbazerque@gmail.com            
;; Nicolas de Galarreta    nicodega@gmail.com
;;
;;    
;; Redistribution and use in source and binary forms, with or without 
;; modification, are permitted provided that conditions specified on 
;; the License file, located at the root project directory are met.
;;
;; You should have received a copy of the License along with the code,
;; if not, it can be downloaded from our project site: sartoris.sourceforge.net,
;; or you can contact us directly at the email addresses provided above.


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
;;    listening_thread_entry:

;; afterwards, the thread enters a perpetual loop reading input 
;; from the keyboard.


;; the register layout during the read routine:

;; cl -> saved interrupt mask
;; bh -> EXT flag
;; bl -> shift-alt-ctrl map

;; bh usage:    bit 0 -> SCROLL LOCK
;;        bit 1 -> NUM LOCK
;;        bit 2 -> CAPS
;;        bit 3 -> EXT

;; bl usage:    bit 0 -> left    shift
;;        bit 1 -> right    shift
;;        bit 2 -> left    ctrl
;;        bit 3 -> right    ctrl
;;        bit 4 -> left    alt
;;        bit 5 -> right    alt

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
;;    Santiago

        
bits 32
    
;; %define simple_mode
;; %define nesting_mode        

%include "../../include/sartoris-i386/gdt-syscalls.h"
    
%define PICm0 0x20        ; the master pic port 0
%define PICm1 0x21        ; the master pic port 1
    
%define EOI   0x20        ; end of interrupt code
    
%define KBD_PORT        0x60
%define KBD_ACK_PORT    0x61

%define KBD_CMD_SETLEDS 0xED
    
%define KBD_BUF_LEN 256
    
%define KEY_UP_CODE 0xf0
%define KEY_EXT_CODE 0xe0
%define KEY_ACK_CODE 0xFA

%define LSHIFT_CODE    0x2a    
%define RSHIFT_CODE    0x36    
%define CTRL_CODE      0x1d
%define ALT_CODE       0x38

%define SCROLL_CODE    0x46
%define NUM_CODE       0x45
%define CAPS_CODE      0x3a

%define LEFT_CODE      0x4b
%define RIGHT_CODE     0x4d
%define UP_CODE        0x48
%define DOWN_CODE      0x50
    
%define LSHIFT 0x01
%define RSHIFT 0x02
%define LCTRL  0x04
%define RCTRL  0x08
%define LALT   0x10
%define RALT   0x20

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
%define EOT 0x03
%define SOT 0x02
%define DEL 0x7F

; bh flags
%define KEY_EXT     8
%define CAPS_LED    4
%define NUM_LED     2
%define SCROLL_LED  1


extern next_read
extern next_write
extern kbd_buf
extern full_buf

global keyb_int_thread_entry
global read_code        ; not really global, but i want to breakpoint
        
section .text

keyb_int_thread_entry:
    
    cli
    xor eax, eax        ; the initial TSS has this registers in zero,
    xor ebx, ebx        ; so this shouldn't be really necessary.
    xor ecx, ecx        ; it's harmless, so we may as well do it.

    mov esi, kbd_buf    ; address of the array used for FIFO output.
    
    ;; we have spare registers!
        
read_code:    

    xor eax, eax
    in al, KBD_PORT        ; read keycode
    mov edx, eax           ; save it

    in al, KBD_ACK_PORT    ; send acknowledge to the keyboard 
    mov ah, al
    or al, 0x80            ; disable keyboard
    out KBD_ACK_PORT, al
    mov al, ah
    out KBD_ACK_PORT, al    ; re-enable keyboard

    mov eax, edx            ; restore keycode
        
    cmp al, KEY_EXT_CODE    ; if this is just an EXT code, 
                            ; set bh and wait (do nothing)
    jne not_ext
    or bh, KEY_EXT
    jmp done
not_ext:                    ; ok, no EXT, check ACK
    cmp al, KEY_ACK_CODE    
    jne not_ack
    call ack_received
    jmp done
not_ack:                    ; no EXT, no ACK, control key?
    and dl, 0x7f
    cmp byte [key2asc+edx], CK
    jnz skip_update        
    call update_control_keys
    and bh, ~(KEY_EXT)
    jmp done
    
skip_update:              ; no control key
    test al, 0x80         ; if this is the release of a key,
    jz key_press          ; do nothing
    and bh, ~(KEY_EXT)    ; we received the ext key already,
                          ; so clear the flag
    jmp done

key_press:                ; ok, so the keyboard is sending an
                          ; actual keycode
            
    test bl, (RSHIFT | LSHIFT)
    jnz shift_pressed
    test bh, (CAPS_LED)
    jz no_shift
shift_pressed:
    ;; if shift is pressed while NUMLOCK is active
    ;; ignore NUMLOCK
    mov edi, shift_key2asc
    jmp done_mode
no_shift:    
        ;; check NUMLOCK flag, if it's on
        ;; and input is from the pad, translate
    test bh, (NUM_LED)
    jz no_numlock
        ;; now, numlock is active, we have to 
        ;; avoid mapping arrows to numbers,
        ;; for that we use the EXT flag
    test bh, KEY_EXT
    jnz no_numlock        ;; not an extended key, ignore NUMLOCK
    mov edi, num_key2asc
    jmp done_mode
no_numlock:
    mov edi, key2asc      ;; this shouldn't be necessary!
done_mode:
    mov dl, [edi+eax]
    cmp dl, NK
    je done
    
    cmp byte [full_buf], 0    ; if our buffer is full, do nothing
    je go           
    and bh, ~(KEY_EXT) 
    jmp done
go:        
    
    mov edi, [next_write]    ; read next write position

    mov [esi+edi], bl
    inc edi

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

    mov al, EOI        
    out PICm0, al        ; acknowledge the interrupt

    pusha        
    call RET_FROM_INT : 0x00000000
    popa
    jmp read_code        ; ok, we're back. let's do it again!
    
    ;; this routine keeps track of the state of the ctrl, alt & shift keys.
    ;; it would make sense to write it in C, but assembly leaves more
    ;; room for useless comments ;)

    ;; you know, i like to express myself during these lonely nights.
    
update_control_keys:
    mov edx, eax
    and al, 0x7f
    
    cmp al, LSHIFT_CODE    ; left shift has changed
    jne no_lshift
    test dl, 0x80
    jz lshift_press
    and bl, ~(LSHIFT)
    ret
lshift_press:
    or bl, LSHIFT
    ret

no_lshift:    
    cmp al, RSHIFT_CODE    ; right shift has changed
    jne no_rshift
    test dl, 0x80
    jz rshift_press
    and bl, ~(RSHIFT)
    ret
rshift_press:
    or bl, RSHIFT
    ret

no_rshift:
    cmp al, CTRL_CODE    ; ctrl, maybe?
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
    cmp al, ALT_CODE    ; alt changed
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
no_alt:                ; led changing control keys
    test dl, 0x80
    jnz no_led         ; ignore key up for led changing CK

    cmp al, NUM_CODE    
    jne no_num
    test bh, NUM_LED
    jz num_on
    and bh, ~(NUM_LED)
    jmp set_l
num_on:
    or bh, NUM_LED
    jmp set_l
no_num:    
    cmp al, CAPS_CODE    
    jne no_caps
    test bh, CAPS_LED
    jz caps_on
    and bh, ~(CAPS_LED)
    jmp set_l
caps_on:
    or bh, CAPS_LED
    jmp set_l
no_caps:
    cmp al, SCROLL_CODE    
    jne no_scroll
    test bh, SCROLL_LED
    jz scroll_on
    and bh, ~(SCROLL_LED)
    jmp set_l
scroll_on:
    or bh, SCROLL_LED
    jmp set_l
no_scroll:
no_led:
    ret

ack_received:
    and     bh, ~(KEY_EXT)
    mov ah, [ack_expected]
    cmp ah, 1
    je set_l2
    cmp ah, 2
    je set_l3
    ret

;; helper label called from update_control_keys 
;; when turning on or off a keyboard led 
set_l:    
    and     bh, ~(KEY_EXT)
    mov     byte [ack_expected], 1
    mov     al, KBD_CMD_SETLEDS
    out    KBD_PORT, al        ; issue set leds command
    ret    

;; called when ack is received and ack_expected == 1
set_l2:
    mov     al, bh
    and    al, 0x7             ; maintain only first three bits (status bits on bh)
    mov     byte [ack_expected], 2
    out    KBD_PORT, al        ; write status bytes
    ret

set_l3:
    mov     byte [ack_expected], 0    ; reset ack
    ret


ack_expected: db 0x00    ;; used for leds status (could use a register, but I'm not sure which are free)
                         ;; 0 no ack expected, 1 ack expected for setting leds, 2 ack expected after stting leds, > 2 nothing

int_mask: db 0xff    
    
    ;; these are the keymaps. they should work reasonably for english 
    ;; at-keyboards. (102-101 keys)
    
key2asc:

db NK, esc, '1', '2', '3', '4', '5', '6'        ; 0x00-0x07
db '7', '8', '9', '0', '-', '=', bs, tab        ; 0x08-0x0f
db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'        ; 0x10-0x17
db 'o', 'p', '[', ']', nl, CK, 'a', 's'            ; 0x18-0x1f
db 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'        ; 0x20-0x27  undef: 0x27
db 0x40, 0x60, CK, '\', 'z', 'x', 'c', 'v'        ; 0x28-0x2f  0x28 (40) is "'", 0x29 is '`' 
db 'b', 'n', 'm', ',', '.', '/', CK, '*'        ; 0x30-0x37
db CK, ' ', CK, F+1, F+2, F+3, F+4, F+5            ; 0x38-0x3f
db F+6, F+7, F+8, F+9, F+10, CK, CK, SOT        ; 0x40-0x47
db UP, '9', '-', LEFT, '5', RIGHT, '+', EOT        ; 0x48-0x4f
db DOWN, '3', '0', DEL, NK, NK, NK, F+11        ; 0x50-0x57 
db F+12
times 7 db NK

shift_key2asc:
db NK, esc, '!', '@', '#', '$', '%', '^'        ; 0x00-0x07
db '&', '*', '(', ')', '_', '+', bs, tab        ; 0x08-0x0f
db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'        ; 0x10-0x17
db 'O', 'P', '{', '}', nl, CK, 'A', 'S'            ; 0x18-0x1f
db 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':'        ; 0x20-0x27  undef: 0x27
db '"', '~', CK, '|', 'Z', 'X', 'C', 'V'        ; 0x28-0x2f  0x28 is """, 0x29 is ~
db 'B', 'N', 'M', '<', '>', '?', CK, '*'        ; 0x30-0x37
db CK, ' ', CK, F+1, F+2, F+3, F+4, F+5            ; 0x38-0x3f
db F+6, F+7, F+8, F+9, F+10, CK, CK, SOT        ; 0x40-0x47
db UP, '9', '-', LEFT, '5', RIGHT, '+', EOT        ; 0x48-0x4f
db DOWN, '3', '0', DEL, NK, NK, NK, F+11        ; 0x50-0x57 
db F+12
times 7 db NK

num_key2asc:     ;; (map exactly as key2asc except for pad keys)
db NK, esc, '!', '@', '#', '$', '%', '^'        ; 0x00-0x07
db '&', '*', '(', ')', '_', '+', bs, tab        ; 0x08-0x0f
db 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'        ; 0x10-0x17
db 'O', 'P', '{', '}', nl, CK, 'A', 'S'            ; 0x18-0x1f
db 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':'        ; 0x20-0x27  undef: 0x27
db '"', '~', CK, '|', 'Z', 'X', 'C', 'V'        ; 0x28-0x2f  0x28 is """, 0x29 is ~
db 'B', 'N', 'M', '<', '>', '/', CK, '*'        ; 0x30-0x37
db CK, ' ', CK, F+1, F+2, F+3, F+4, F+5            ; 0x38-0x3f
db F+6, F+7, F+8, F+9, F+10, CK, CK, '7'        ; 0x40-0x47
db '8', '9', '-', '4', '5', '6', '+', '1'        ; 0x48-0x4f
db '2', '3', '0', '.', NK, NK, NK, F+11        ; 0x50-0x57 
db F+12
times 7 db NK


