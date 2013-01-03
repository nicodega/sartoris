
	;; Sartoris i386 kernel vga mini-driver
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com      

global conf_vga

seq_reg_adr	equ	0x3c4
seq_reg_data	equ	0x3c5
graph_reg_adr	equ	0x3ce
graph_reg_data	equ	0x3cf
crt_reg_adr	equ	0x3D4
crt_reg_data	equ	0x3D5
	
	
conf_vga:
	mov dx, graph_reg_adr
	mov al, 0x6		; pido el reg grafico 6
	out dx, al
	mov dx, graph_reg_data
	in al, dx		; leo el contenido del index grafico 6
	and al, 254		; habilito el alfanumerico
	or al, 0xE 		; pongo la dir de memoria B8000 a BFFFF
	out dx, al		; lo escribo

	mov dx, graph_reg_adr
	mov al, 0x5		; pido el reg grafico 5
	out dx, al
	mov dx, graph_reg_data
	in al, dx		; leo el contenido del index grafico 5
	and al, 244		; write mode = 0, read mode 0
	or al, 16		; Host O/E = 1
	out dx, al		; lo escribo 
	
	mov dx, graph_reg_adr
	mov al, 0x3		; pido el reg grafico 3
	out dx, al
	mov dx, graph_reg_data
	in al, dx		; leo el contenido del index grafico 3
	and al, 224		; RotateCount=0
				; LogicalOp= none
	out dx, al		; lo escribo

	mov dx, seq_reg_adr
	mov al, 0x2		; pido el reg 2 del sequencer 
	out dx, al
	mov dx, seq_reg_data
	in al, dx
	and al, 240		; MemoryPlaneWriteEnable = 0011
	or al, 3		; (habilito la escritura en 0 y 1)
	out dx, al		; lo escribo 

	mov dx, seq_reg_adr
	mov al, 0x4		; pido el reg 4 del sequencer 
	out dx, al
	mov dx, seq_reg_data
	in al, dx
	and al, 0xfa		; O/E Dis = 0
	or al, 2		; ExtMem = 1
	out dx, al		; lo escribo

	ret
