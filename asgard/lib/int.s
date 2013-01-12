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


bits 32
	
	;; until we have mutex in the process manager, i need a way to obtain mutual exclusivity

global enter_block
global exit_block
global ack_int_master

extern eoi
	
enter_block:   
    pushf
    pop eax
    and eax, 0x00000200
	cli
	ret

exit_block:
    mov eax, [esp+4]
    and eax, 0x00000200
    jz exit_block_no_sti
	sti
exit_block_no_sti:
	ret

ack_int_master:		
	call eoi
	ret

