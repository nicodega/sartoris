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

global _ldstart

;; void __ldmain(struct init_data *initd) on ld.c
extern __ldmain
;; void __ldexit() on ld.c
extern __ldexit

;; Process manager will leave the stack in this way: 
;;
;;		 ----------------------------
;;		| ldinit struct (size bytes) | <-- will be upside down in memory
;;		|                            | 
;;		|----------------------------|
;;		| ldinit struct size		 | <-- esp at the start of the stack!  
;;		 ----------------------------

bits 32

;; . <-- this is LDs entry point!
_ldstart:
	mov eax, esp
    mov ebx, esp
	sub eax, [esp]		  ;; substract size bytes from esp possition
	sub esp, [esp]		  ;; position at the begining of init structure [esp] = struct size

	sub esp, 4			  ;; leave 4 bytes to the stack (don't remember why!)
		
	push eax			  ;; push the pointer to init structure
	mov edx, [eax+4]      ;; we where relocated, add offset to the function address.
    add edx, __ldmain
	call edx

	mov esp, ebx          ;; restore the stack initial position
    mov dword [esp], 5*4  ;; change structure size to the init_data size
    mov eax, esp
    sub eax, 5*4
    mov [eax], dword __ldexit
    call [eax]            ;; execute the thread entry point
    
	jmp $ ;; <-- we will never reach this point, but if we do, we cannot return
	
	