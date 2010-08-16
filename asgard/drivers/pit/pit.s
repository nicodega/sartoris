
;;     Oblivion 0.1 PIT support driver
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

%define PIT_OSCILATOR_FREQ 1193180

%define PIT_CTRL_PORT 0x43
%define PIT_COUNTER_PORT 0x40

%define PIT_CONTROL_WORD 0x36

global adjust_pit

bits 32

section .text

;; This function will set mode 3, using a binary counter
;; indicating firs value will be written first and then 
;; second value, using counter 0.

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

    
