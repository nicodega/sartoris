    
;; Sagard 0.1 mouse driver
;;
;; Copyright (C) 2011
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


;; this thread will handle IRQ12, the PS2 mouse!.
        
bits 32
   
%include "../../include/sartoris-i386/gdt-syscalls.h"
    
%define PIC0_CTRL 0x20           ;; PIC0 i/o address
%define PIC0_MASK 0x21           ;; PIC0 i/o address
%define PIC1_CTRL 0xA0           ;; PIC1 i/o address
%define PIC1_MASK 0xA1           ;; PIC1 i/o address
    
%define EOI   0x20        ; end of interrupt code
   
;; The PS2 mouse shares the controler with the keyboard. While whe handle
;; the mouse interrupt, the keyboard won't be able to process keys, so let's
;; try to make this handler FAST.
;; 
;;

global mouse_int_handler
global mouse_changed
global mouse_deltax
global mouse_deltay
global state
global estate
global mouse_init

%define DATA_PORT 0x60
%define CTRL_PORT 0x64

section .text

mouse_changed:
    db 0        ;; this flag will be set when something changes on the mouse
mouse_cycle: 
    db 0        ;; ammount of bytes read from the mouse
mouse_cycles:
    db 3        ;; by default we will support 3 bytes
mouse_deltax:
    dd 0        ;; movement on X.
mouse_deltay:
    db 0        ;; movement on Y.
state:
    db 0
estate:
    db 0
    
;; This is the mouse interrupt handler.
mouse_int_handler:
    cli
    xor eax, eax
    xor ecx, ecx

mouse_do_int:
    in al, DATA_PORT
    mov cl, [mouse_cycle]
    cmp cl, 1
    ja above1
    jb below1
    ;; it's 1
    mov byte [mouse_deltax], al     ;; cycle 1
    inc cl    
    jmp mouse_int_cont
above1:
    cmp cl, 2
    ja cycle3  
    mov byte [mouse_deltay], al     ;; cycle 2
    cmp byte [mouse_cycles], 3
    je finished
    inc cl
    jmp mouse_int_cont
cycle3:
    mov byte [estate], al    ;; cicle 3
finished:
    xor cl, cl
    mov byte [mouse_changed], 1
    jmp mouse_int_cont
below1:
    mov byte [state], al     ;; cycle 0   
    inc cl
mouse_int_cont:
    mov byte [mouse_cycle], cl
    mov al, EOI        
    out PIC0_CTRL, al        ; acknowledge the interrupt
    mov al, EOI        
    out PIC1_CTRL, al        ; acknowledge interrupt on pic1 (int 12 is on the second pic)
    call RET_FROM_INT : 0x00000000
    jmp mouse_do_int        ; ok, we're back. let's do it again!

;; This function provides timeut for write
;; Timeout type is specified on dl. 
;; These registers won't be preserved: ebx, al
mouse_wait:
    mov ebx, 1001
    cmp dl, 1
    jne type2
type1:
    dec ebx
    jz timed_out
    in al, CTRL_PORT
    and al, 1
    jz type1
    jmp timed_out
type2:
    dec ebx
    jz timed_out
    in al, CTRL_PORT
    and al, 2
    jnz type2
timed_out:
    ret

;; write a byte on the mouse IO port
;; byte to be written must be on cl
;; These registers won't be preserved: dl, al, ebx, dl 
mouse_write:
    mov dl, 1
    call mouse_wait
    mov al, 0xD4
    out CTRL_PORT, al
    mov dl, 1
    call mouse_wait
    mov al, cl
    out DATA_PORT, al
    ret
    
;; will read the mouse port.
;; the value is returned on al.
mouse_read:
    mov dl, 0
    call mouse_wait
    in al, DATA_PORT
    ret

;; this function can be invoked from C.
;; mouse_init(byte resolution, byte scaling, byte sampling_rate)
mouse_init:
    push ebp
    mov ebp, esp
    push ebx

    ;; enable auxiliary mouse device
    mov dl, 1
    call mouse_wait
    mov al, 0xA8
    out CTRL_PORT, al
        
    ;; initialize the mouse to default settings
    ;; to have it on a known state
    mov cl, 0xF6
    call mouse_write
    ;; read ack
    call mouse_read

    ;; see if it has a wheel
    call detect_wheel

    ;; configure resolution
    cmp byte [ebp+8], 0
    je no_resolution
    mov cl, 0xE8
    call mouse_write
    call mouse_read  ;; read ack
    mov cl, [ebp+8]
    call mouse_write
    call mouse_read  ;; read ack
no_resolution:
    ;; configure scaling
    cmp byte [ebp+12], 0
    je no_scaling
    cmp byte [ebp+12], 1
    je scaling11
    mov cl, 0xE7
    call mouse_write
    call mouse_read  ;; read ack
    jmp no_scaling
scaling11:
    mov cl, 0xE6
    call mouse_write
    call mouse_read  ;; read ack
no_scaling:
    ;; configure samplig rate
    cmp byte [ebp+16], 0
    je no_samplingr
    mov cl, 0xF3
    call mouse_write
    call mouse_read  ;; read ack
    mov cl, [ebp+16]
    call mouse_write
    call mouse_read  ;; read ack
no_samplingr:

    ;; enable interrupts for the mouse
    mov dl, 1
    call mouse_wait
    mov al, 0x20
    out CTRL_PORT, al

    mov dl, 0
    call mouse_wait
    in al, DATA_PORT
    or ax, 2
    push ax

    mov dl, 1
    call mouse_wait
    mov al, 0x60
    out CTRL_PORT, al

    mov dl, 1
    call mouse_wait
    pop ax
    out DATA_PORT, al

    ;; enable mouse packets
    mov cl, 0xF4
    call mouse_write
    call mouse_read
    
    pop ebx
    pop ebp
    ret

detect_wheel:
    ;; send the magic secuence for the wheel
    ;; sampling 200
    ;; sampling 100
    ;; sampling 80
    ;; check id

    mov cl, 0xF3
    call mouse_write
    call mouse_read  ;; read ack
    mov cl, 200
    call mouse_write
    call mouse_read  ;; read ack

    mov cl, 0xF3
    call mouse_write
    call mouse_read  ;; read ack
    mov cl, 100
    call mouse_write
    call mouse_read  ;; read ack

    mov cl, 0xF3
    call mouse_write
    call mouse_read  ;; read ack
    mov cl, 80
    call mouse_write
    call mouse_read  ;; read ack
    ;; read the id
    mov cl, 0xF2
    call mouse_write
    call mouse_read  ;; read ack
    call mouse_read  ;; read id
xchg bx, bx
    cmp al, 3
    jne no_wheel
    cmp al, 4
    jne no_wheel
    mov byte [mouse_cycles], 4
no_wheel:
    ret
