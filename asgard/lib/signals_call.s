;;	Copyright (C) 2002, 2003, 2004, 2005
;;       
;;	Santiago Bazerque 	sbazerque@gmail.com			
;;	Nicolas de Galarreta	nicodega@gmail.com
;;
;;	
;;	Redistribution and use in source and binary forms, with or without 
;; 	modification, are permitted provided that conditions specified on 
;;	the License file, located at the root project directory are met.
;;
;;	You should have received a copy of the License along with the code,
;;	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
;;	or you can contact us directly at the email addresses provided above.

;; 
;; This file defines _start and __start symbols, and moves
;; the stack pointer, so init won't step over the init_data 
;; structure
;;

bits 32

;; This file implements call_with_ss_swap function.
;; This function will call the address specified by
;; addr, swapping the stack to the one on ss param.

;; call_with_ss_swap(void *addr, void *ss)

call_with_ss_swap:

    mov eax, [esp + 4]  ;; put addr on eax
    mov edx, [esp + 8]  ;; put our new stack addr on edx
    
    push ebp
    mov ebp, esp    ;; preserve our old esp on ebp

    ;; swap stack
    mov esp, edx

    ;; call the function
    call eax

    ;; get our stack back
    mov esp, ebp
    pop ebp 
    
    xor eax, eax
    ret
