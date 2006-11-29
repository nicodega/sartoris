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

global _start
global __start

;; void __procinit(struct init_data *initd) on init.c
extern __procinit

;; Process manager will leave the stack in this way: 
;;
;;		 --------------------------
;;		| init struct (size bytes) | <-- will be upside down in memory
;;		|                          | 
;;		|--------------------------|
;;		| init struct size		   | <-- esp at the start of the stack!  
;;		 --------------------------

bits 32

_start:
__start:

	mov eax, esp
	sub eax, [esp]		;; substract size bytes from esp possition
	sub esp, [esp]		;; position at the begining of init structure [esp] = struct size

	sub esp, 4			;; leave 4 bytes to the stack
		
	push eax			;; push the pointer to init structure
	call __procinit
	
	jmp $ ;; <-- we will never reach this point, but if we do, we cannot return
	
	