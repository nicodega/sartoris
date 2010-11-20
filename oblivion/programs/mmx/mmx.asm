bits 32

main:
	movd  mm1, [val]
	jmp main
	
val:
	dd 0xFEFEAEAE
	
init_msg:
	dd 0
	dd 0
	dd 0
	dd 0
