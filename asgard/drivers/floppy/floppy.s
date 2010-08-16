
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

global disk_change
global set_data_rate
global set_floppy
global busy
global get_msr
global get_data
global send_data
global req
global on_command
global disk_present

;/* IO ports */
%define FDC_DOR  (0x3f2)   ;/* Digital Output Register */
%define FDC_MSR  (0x3f4)   ;/* Main Status Register (input) */
%define FDC_DRS  (0x3f4)   ;/* Data Rate Select Register (output) */
%define FDC_DATA (0x3f5)   ;/* Data Register */
%define FDC_DIR  (0x3f7)   ;/* Digital Input Register (input) */
%define FDC_CCR  (0x3f7)   ;/* Configuration Control Register (output) */
 
    
;***********************************
; void send_data(int d)
;***********************************

send_data:
    push ebp
    mov ebp, esp
    push edx
    
    mov eax, [ebp + 8]

    mov dx, FDC_DATA
    out dx, al

    pop edx
    pop ebp
    ret    

;***********************************
; int get_data()
;***********************************

get_data:
    push ebp
    mov ebp, esp
    push edx

    xor eax, eax
    mov dx, FDC_DATA
    in al, dx

    pop edx
    pop ebp
    ret


;***********************************
; int on_command()
;***********************************

on_command:
    push ebp
    mov ebp, esp
    push edx

    xor eax, eax    
    mov dx, FDC_MSR
    in al, dx
    and al, 0x10    ; nonzero if on command

    pop edx
    pop ebp
    ret

;***********************************
; int busy()
;***********************************

busy:
    push ebp
    mov ebp, esp
    push edx

    xor eax, eax    
    mov dx, FDC_MSR
    in al, dx
    and al, 0xc0        
    cmp al, 0x80
    je busy1
    mov al, 1
    jmp busy2
busy1:
    mov al, 0
busy2:
    pop edx
    pop ebp
    ret

;***********************************
; int req()
;***********************************
req:
    push ebp
    mov ebp, esp
    push edx

    xor eax, eax    
    mov dx, FDC_MSR
    in al, dx
    and al, 0xd0        
    cmp al, 0xd0
    je req1
    mov al, 0
    jmp req2
req1:
    mov al, 1
req2:
    pop edx
    pop ebp
    ret

;***********************************
; void set_floppy(int flags)
;***********************************

set_floppy:
    push ebp
    mov ebp, esp
    push edx
    
    ; get flags
    
    mov eax, [ebp + 8]    

    mov dx, FDC_DOR
     out dx, al

    pop edx
    pop ebp
    ret

;**********************************
; void set_data_rate(int flags)
;**********************************

set_data_rate:
    push ebp
    mov ebp, esp
    push edx
    
    mov eax, [ebp + 8]

    mov dx, FDC_CCR        ; program data rate
    out dx, al

    pop edx
    pop ebp
    ret

;**********************************
; int disk_change() 
;
; ret: 1 true or 0 false
;
; NOTE: motor has to be on to call 
;    this function
;**********************************

disk_change:
    push ebp
    mov ebp, esp
    push edx
    
    xor eax, eax
    mov dx, FDC_DIR
    in al, dx
    shr al, 7
    
    pop edx
    pop ebp
    ret 
