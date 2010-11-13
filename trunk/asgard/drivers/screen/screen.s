
;;     Text Mode VGA driver routines.
;;
;;    Copyright (C) 2002, 2003, 2004, 2005
;;       
;;    Santiago Bazerque     sbazerque@gmail.com            
;;    Nicolas de Galarreta    nicodega@gmail.com
;;
;;    
;;    Redistribution and use in source and binary forms, with or without 
;;     modification, are permitted provided that conditions specified on 
;;    the License file, located at the root project directory are met.
;;
;;    You should have received a copy of the License along with the code,
;;    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
;;    or you can contact us directly at the email addresses provided above.

    
bits 32
    
%define screen_base 0x18000

seq_reg_adr    equ    0x3c4
seq_reg_data   equ    0x3c5
graph_reg_adr  equ    0x3ce
graph_reg_data equ    0x3cf
crt_reg_adr    equ    0x3D4
crt_reg_data   equ    0x3D5
    

global clear_screen
global scroll_up
global scroll_up_one
global string_print
global set_cursor_pos
global cursor_on
global cursor_off
global read_screen
global write_screen
    
section .text

;; clear the screen 
clear_screen:
    mov ecx, 25*80*2
clear_loop:
    sub ecx, 4
    mov dword [fs:ecx+screen_base], 0x07200720    ; 07 = standard color 
    jnz clear_loop

    ret

;; scroll up n lines
scroll_up:
    push ebp
    mov ebp, esp
    push esi
    
    mov eax, [ebp+8]
    imul eax, 160
    mov esi, 24*80*2
    sub esi, eax
    xor ecx, ecx
scroll_loop:
    mov edx, [fs:ecx+eax+screen_base]
    mov [fs:ecx+screen_base], edx
    add ecx, 4
    cmp ecx, esi
    jb scroll_loop

    pop esi
    pop ebp
    ret


;; scroll up one line
scroll_up_one:
    
    xor ecx, ecx
    mov edx, (24*80*2)
scroll_one_loop:
    mov eax, [fs:screen_base+ecx+80*2]
    mov [fs:screen_base+ecx], eax
    add ecx, 4
    cmp ecx, edx
    jnz scroll_one_loop
    
    mov ecx, 80*2
clear_last_loop:
    sub ecx, 4
    mov dword [fs:ecx+screen_base+80*24*2], 0x07200720    ; 07 = standard color 
    jnz clear_last_loop
    
    ret
    
    
;; print zero terminated string x in screen position y, using attribute z 
;; The function places the cursor at the end of the printed text
string_print:
    push ebp
    mov ebp, esp
    push ebx

    mov ecx, [ebp+8]     ; source
    mov edx, [ebp+12]    ; destination
    mov eax, [ebp+16]    ; atribute
    shl eax, 8
    
print_loop:
    mov al, [ecx]
    cmp al, 0
    jz print_done
    mov [fs:edx+screen_base], ax
    add ecx, 1
    add edx, 2
    jmp print_loop

print_done:
%ifdef auto_cursor    
    ;; now set the cursor on the last position
    shr edx, 1
    mov ebx, edx
    call set_cursor_pos_offset
%endif
    pop ebx
    pop ebp
    ret

    ;; this function reads the contents of the vga memory area to a buffer

read_screen:
    push ebp
    mov ebp, esp
    mov edx, [ebp+8]
    mov ecx, (80*25*2)
read_loop:
    sub ecx, 4
    mov eax, [fs:screen_base+ecx]
    mov [edx+ecx], eax
    jnz read_loop

    pop ebp
    ret

    ;; this function writes the contnents of the vga memory area 
    ;; from a buffer

write_screen:
    push ebp
    mov ebp, esp
    mov edx, [ebp+8]
    mov ecx, (80*25*2)
write_loop:
    sub ecx, 4
    mov eax, [edx+ecx]
    mov [fs:screen_base+ecx], eax
    jnz write_loop

    pop ebp
    ret
    
; VIDEO CURSOR FUNCTIONS
    
set_cursor_pos:
    push ebp
    mov ebp, esp
    push ebx
    
    ; calculate the cursor offset (x + 80*y)
    ; NOTE: this function assumes the x and y values are valid.

%ifdef x_plus_y        
    mov eax, 80        ; screen width
    mul dword [ebp+12]
    mov ebx, [ebp+8]
    add ebx, eax
%endif
    mov ebx, [ebp+8]
    
    call set_cursor_pos_offset
    
    pop ebx
    pop ebp
    ret

;; this function gets the offset in ebx 
set_cursor_pos_offset:    
    ; truncate the offset to get a 16 bit offset
    ; and write it to the VGA registers

    mov dx, crt_reg_adr
    mov al, 0x0E
    out dx, al
    mov dx, crt_reg_data
    mov al, bh
    out dx, al

    mov dx, crt_reg_adr
    mov al, 0x0F
    out dx, al
    mov dx, crt_reg_data
    mov al, bl
    out dx, al
    
    ret

cursor_on:
    push ebp
    mov ebp, esp
    
    ; set the cursor shape

    ;  first we get the maximum scanline
    mov dx, crt_reg_adr    ; 
    mov al, 0x09
    out dx, al        
    mov dx, crt_reg_data
    in al, dx         ; read crt register 09h
    mov ah, al        
    and ah, 31        ; now we have the Maximum scanline in ah
    
    ; change the Cursor Scan Line Start value to the Maximum scanline
    ; and set the "Cursor enabled bit" on the CRT reg to 0.

    ; zero Cursor Skew and set Cursor Scan Line End 

    mov dx, crt_reg_adr
    mov al, 0x0B
    out dx, al
    mov dx, crt_reg_data
    xor al, al        ; cero everything
    or al, ah         ; set the Cursor Scan Line End
    out dx, al        ; write it

    dec ah
    
    mov dx, crt_reg_adr
    mov al, 0x0A
    out dx, al
    mov dx, crt_reg_data
    in al, dx        ; read crt register 0Ah
    and al, 192      ; Enable Cursor and clear the first bits
    or al, ah        ; set the Cursor Scan Line Start
    out dx, al       ; write it again

    pop ebp
    ret

cursor_off:
    push ebp
    mov ebp, esp
    push edx
    push eax    

    ; set the "Cursor enabled bit" on the CRT reg to 1.

    mov dx, crt_reg_adr
    mov al, 0x0A
    out dx, al
    mov dx, crt_reg_data
    in al, dx        ; read crt register 0Ah
    or al, 32        ; Disable Cursor
    out dx, al       ; write it again

    pop eax
    pop edx
    pop ebp
    ret

