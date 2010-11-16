
global reboot

reboot:
	call empty_kb_buf

	mov al, 0xfe
	out 0x64, al
die:	jmp die


disable_a20:		
	call empty_kb_buf	; enable A20. first we empty the keyboard buffer
	mov al, 0xd1
	out 0x64, al		; 0xd1 -> port 0x64: write the following data
				; byte to the output port
	call empty_kb_buf
	mov al, 0xdd		; i think bit 1 of the outputport is the A20, but sending
	out 0x60, al		
	
	call empty_kb_buf	; A20 up, hopefully
	ret	

empty_kb_buf:			; this is the standard way, i belive.
	in al, 0x64
	and al, 0x02
	jnz empty_kb_buf
	ret
