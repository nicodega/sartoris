
;;    Copyright (C) 2002, 2003, 2004, 2005
;;       
;;    Santiago Bazerque     sbazerque@gmail.com            
;;    Nicolas de Galarreta    nicodega@gmail.com
;;
;;    
;;    Redistribution and use in source and binary forms, with or without 
;;     modification, are permitted provided that conditions specified on 
;;    the License file, located at the root project directory are met.
;;
;;    You should have received a copy of the License along with the code,
;;    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
;;    or you can contact us directly at the email addresses provided above.



bits 32

global set_dma_channel
global init_dma

%define page    0
%define offset    1
%define length  2
%define mask    3
%define cmode    4
%define clear    5

;;************************************
;; function to initialize DMA
;;************************************
;; void init_dma(int control) 

%define control [ebp + 8]
init_dma:
    push ebp
    mov ebp, esp
    push eax    
    
    ;;  enable DMA controllers (including the 16bit dma)

    mov eax, control
    out 0x08, al
    out 0xd0, al    

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
    
    cli    ;; disable irq's 
    
    ;; set the mask bit for the channel
    ;; outb(0x0a,channel | 4) 
    xor edx, edx
    mov ebx, channel
    mov dl, [dmainfo + ebx*8 + mask]
    mov eax, 4
    or eax, ebx
    out dx, al
    
    ;; clear flipflop
    ;; outb(0x0c,0)
    mov dl, [dmainfo + ebx*8 + clear]
    xor eax, eax
    out dx, al
    
    ;; set DMA mode
    ;; // outb(0x0b,(read ? 0x44 : 0x48) + channel)
    ;; ebx has the channel
    xor dx, dx
    mov dl, [dmainfo + ebx*8 + cmode]
    mov eax, ebx    
    or eax, mode
    out dx, al

    ;; set DMA page
    ;; outb(dmainfo[channel].page,page)

    mov ecx, physaddr
    mov eax, ecx
    shr eax, 16
    xor edx, edx
    mov dl, [dmainfo + ebx*8 + page]
    out dx, al
    
    ;; set DMA offset 
    ;outb(dmainfo[channel].offset,offset & 0xff);   low byte 
    ;outb(dmainfo[channel].offset,offset >> 8);     high byte 

    mov eax, ecx        
    mov dl, [dmainfo + ebx*8 + offset]
    out dx, al
    mov al, ah
    out dx, al
    
    ; set DMA length
    ;outb(dmainfo[channel].length,length & 0xff);   low byte 
    ;outb(dmainfo[channel].length,length);     high byte 

    mov eax, lengthp
    dec eax            ; size - 1
    mov dl, [dmainfo + ebx*8 + length]
    out dx, al
    mov al, ah
    out dx, al
    
    ;; clear DMA mask bit 
    ;; outb(0x0a,channel)
    mov dl, [dmainfo + ebx*8 + mask]
    mov eax, ebx
    out dx, al
    
    sti

    pop ebx
    pop ecx
    pop edx
    pop eax
    pop ebp
    ret

;; DMA channel properties
    
;;     page  addr  count mask  mode  clear
dmainfo:    
    db 0x87, 0x00, 0x01, 0x0A, 0x0B, 0x0C, 0, 0            ;; channel 0
    db 0x83, 0x02, 0x03, 0x0A, 0x0B, 0x0C, 0, 0
    db 0x81, 0x04, 0x05, 0x0A, 0x0B, 0x0C, 0, 0
    db 0x82, 0x06, 0x07, 0x0A, 0x0B, 0x0C, 0, 0
    db 0x8F, 0xC0, 0xC2, 0xD4, 0xD6, 0xD8, 0, 0
    db 0x8B, 0xC4, 0xC6, 0xD4, 0xD6, 0xD8, 0, 0
    db 0x89, 0xC8, 0xCA, 0xD4, 0xD6, 0xD8, 0, 0
    db 0x8A, 0xCC, 0xCE, 0xD4, 0xD6, 0xD8, 0, 0            ;; channel 7
    
    
