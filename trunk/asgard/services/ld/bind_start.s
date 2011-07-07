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

extern _GLOBAL_OFFSET_TABLE_
extern dl_runtime_bind
global dl_runtime_bind_start

bits 32

dl_runtime_bind_start:
    call .get_GOT
.get_GOT: 
    pop     ebx 
    add     ebx,_GLOBAL_OFFSET_TABLE_+$$-.get_GOT wrt ..gotpc 
	pushf				
	push   eax
	push   ecx
	push   edx
	push   ebx
	push   ebp
	push   esi
	push   edi
	push   ds
	push   es
	push   dword [esp +44]      ;; offset
	push   dword [esp +44]		;; object pointer
    call   dl_runtime_bind WRT ..plt
	add    esp, 8
    mov    [esp+44], eax        ;; set the function to be called on 
                                ;; the object possition, so it will run on ret.
	
	pop    es
	pop    ds
	pop    edi
	pop    esi
	pop    ebp
	pop    ebx
	pop    edx
	pop    ecx
	pop    eax
	popf

	lea    esp, [esp + 4]
	ret

	