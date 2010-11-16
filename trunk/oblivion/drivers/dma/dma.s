bits 32

global set_dma_channel
global init_dma

%define page	0
%define offset	1
%define length  2

;;************************************
;; function to initialize DMA
;;************************************
;; void init_dma(int control) 

%define control [ebp + 8]
init_dma:
	push ebp
	mov ebp, esp
	push eax	
	
	;;  enable DMA controller

	mov eax, control
	out 08h, al

	pop eax
	pop ebp
	ret

;;************************************
;; function to transfer data with DMA
;;************************************
;; void set_dma_channel(int channel, long physaddr, int length, int mode)
	
set_dma_channel:	
	push ebp
	mov ebp, esp
	push eax
	push edx
	push ecx
	push ebx

	%define channel [ebp + 8]
	%define physaddr [ebp + 12]
	%define lengthp [ebp + 16]
	%define mode [ebp + 20]
	
	cli	;; disable irq's 
	
	;; set the mask bit for the channel
	;; outb(0x0a,channel | 4) 
	mov ebx, channel
	mov eax, 4
	or eax, ebx
	out 0x0a, al
	
	;; clear flipflop
	;; outb(0x0c,0)
	xor eax, eax
	out 0x0c, al
	
	;; set DMA mode
	;; // outb(0x0b,(read ? 0x44 : 0x48) + channel)
	;; ebx has the channel
	mov eax, ebx	
	or eax, mode
	out 0x0b, al

	;; set DMA page
	;; outb(dmainfo[channel].page,page)

	mov ecx, physaddr
	mov eax, ecx
	shr eax, 16
	xor edx, edx
	mov dl, [dmainfo + ebx*4 + page]
	out dx, al
	
	;; set DMA offset 
	;outb(dmainfo[channel].offset,offset & 0xff);   low byte 
	;outb(dmainfo[channel].offset,offset >> 8);     high byte 

	mov eax, ecx		
	mov dl, [dmainfo + ebx*4 + offset]
	out dx, al
	mov al, ah
	out dx, al
	
	; set DMA length
	;outb(dmainfo[channel].length,length & 0xff);   low byte 
	;outb(dmainfo[channel].length,length >> 8);     high byte 

	mov eax, lengthp
	dec eax			; size - 1
	mov dl, [dmainfo + ebx*4 + length]
	out dx, al
	mov al, ah
	out dx, al
	
	;; clear DMA mask bit 
	;; outb(0x0a,channel)
	mov eax, ebx
	out 0x0a, al
	
	sti

	pop ebx
	pop ecx
	pop edx
	pop eax
	pop ebp
	ret

;; DMA channel properties
	
dmainfo:	
	db 0x87, 0x00, 0x01, 0
	db 0x83, 0x02, 0x03, 0
	db 0x81, 0x04, 0x05, 0
	db 0x82, 0x06, 0x07, 0
