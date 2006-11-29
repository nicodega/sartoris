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

%define PICm0 0x20		; the master pic port 0
%define PICm1 0x21		; the master pic port 1

bits 32

global killsched	

killsched:	
	in al, PICm1 		; read current mask (for master)
	
	or al, 0x01		; disable the scheduler  
				;(IRQ 0, master)
	out PICm1, al

	ret
