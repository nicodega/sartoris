
;; note: if execseg16 is changed, make sure to update the _gdt_pseudo_descr
;;       to point to the new GDT in flat mode. santiago.


;;
;; This will conform sartoris Stage 2 boot sector. This stage is 
;; (Almost, because we won't support video modes changing) Multiboot conforming.
;;

%include "multiboot.inc"
%include "stages.inc"

%define stage_2_blocks 1
   
%define realaddress(x) (x + (stage2seg16*0x10-bootseg16*0x10))

stage2_entry:

	;; stage 2 has been loaded completely in memory
	;; NOTE: On Sartoris Stage 2 is supposed to contain the init image.
	
	;; stage 1.5 loader must have left:
	;; ds:si = segment:offset of disk address packet/drive geometry
	;; dl = drive num
	;; ax = stage 2 image size
	;; this must have been left this way by the 1.5
	;; loader
	
	;; sartoris Stage 2 loader is not as complex as a real
	;; boot loader like grub. This is only ment to cover the
	;; Multiboot procedure, and if a real bootloader 
	;; is needed (for selecting different kernel versions, etc)
	;; this one cannot be used.
	
	;; cx will hold image size (in blocks) without stage 2 loader size
	mov cx, ax 
	sub cx, stage_2_blocks
	
	;; ds:si will point to stage2seg16:0
	mov ax, stage2seg16
	mov ds, ax
	xor si, si
	
	;; bx will point to the Multiboot header 
	xor ebx, ebx
	mov bx, stage_2_blocks
	shl bx, 9				;; multiply by 512
	
	;; check first value is the magic number
	cmp dword [bx], MULTIBOOTH_MAGIC
	jne near error
	
	;; use checksum for checking flags and magic number
	mov eax, [bx]
	add eax, [bx + 4]
	add eax, [bx + 8]
	
	cmp eax, 0
	jne near error
	
	;; FIXME: we should check flags, but we already know what sartoris 
	;; needs, so it should be added here.
	
	;; setup things so the image can be run

;; enable A20
	
	call empty_kb_buf	

	mov al, 0xd1
	out 0x64, al		; 0xd1 -> port 0x64: write the following data
				; byte to the output port
	call empty_kb_buf
	mov al, 0xdf		; i think bit 1 of the outputport is the A20, 
	out 0x60, al		; but sending 0xdf seems to work. need info on
				; this.
	
	call empty_kb_buf	; A20 up, hopefully

;; memory size

	;; attempt getting memory size from BIOS
	
	
		
;; switch to p-mode

	cli
	
	lgdt [_gdt_pseudo_descr]
	lidt [_idt_pseudo_descr]
	
	mov eax, cr0
	or eax, 1
	mov cr0, eax

	jmp dword 0x0008:(stage2seg16*0x10 + go32)		; mama!
go32:		
bits 32

	mov ax, 0x0010
	mov ds, ax 
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax

	;; setup Multiboot info struct
	mov edi, mbootinfaddr
	
	mov eax, 0x0
	mov [edi], eax
	
	;; the header on sartoris is placed at the begining of the kernel image
	;; so multiboot_header.header_addr tell us where we should load the 
	;; image
	
	mov ebx, stage2addr + stage_2_blocks*512

	;; move the kernel image to its position
	xor eax, eax
	mov ax, cx			;; cx has image size in blocks, without stage loader 2 size
	shl eax, 7			;; multiply by 512 / 4
	
	mov ecx, eax
	mov esi, [ebx + multiboot_header.header_addr]	; from
	sub esi, [ebx + multiboot_header.load_addr]
	add esi, ebx
	mov edi, [ebx + multiboot_header.header_addr]	; to
	cld
	rep
	movsd
	
;; EAX contains the magic value 0x2BADB002
;; EBX contains the 32-bit physical address of the Multiboot information structure provided by the boot loader. 
;; CS is a 32-bit read/execute code segment with an offset of 0 and a limit of 0xFFFFFFFF. The exact value is undefined. 
;; DS,ES,FS,GS,SS are 32-bit read/write data segment with an offset of 0 and a limit of 0xFFFFFFFF. The exact values are all undefined. 
;;
;; A20 gate is enabled. 
;; CR0 Bit 31 (PG) is cleared. Bit 0 (PE) is set. Other bits are all undefined. 
;; EFLAGS 
;;		Bit 17 (VM) is cleared. Bit 9 (IF) is cleared. Other bits are all undefined. 
;;		All other processor registers and flag bits are undefined. 

	mov edx, [ebx + multiboot_header.entry_addr]

	mov eax, MULTIBOOTH_MAGIC
	mov ebx, mbootinfaddr
	
	jmp edx		; this should jump to 0x8:edx

bits 16

error:
	jmp $

empty_kb_buf:			; this is the standard way, i belive.
	in al, 0x64
	and al, 0x02
	jnz empty_kb_buf
	ret


;; this system tables are temporary, the kernel will
;; update them later

gdt:	

dw 0, 0, 0, 0	; dummy descriptor
	
dw 0xffff ; limit=ffff	
dw 0x0000 ; base_adress=0
dw 0x9a00 ; p-flag=1, dpl=0, s-flag=1 (non-system segment), type code exec/read
dw 0x00cf ; g-flag=1, d/b bit=1	(limit mult by 4096, default 32-bit opsize), limit upper bits F

dw 0xffff ; limit=ffff
dw 0x0000 ; base_adress=0
dw 0x9200 ; p-flag=1, dpl=0, s-flag=1, type read/write
dw 0x00cf ; g-flag=1, d/b bit=1	, limit upper bits F
	
_gdt_pseudo_descr:	
	dw 0x0030		; gdt_limit=48 (3 descriptors)
	dw (gdt + (stage2seg16 & 0x0FFF)), (stage2seg16 >> 12)
	
_idt_pseudo_descr:	
	dw 0x0000		; interrupts will be desabled until 
	dw 0x0000, 0x0000	; the real system tables are ready.
	
times ((stage_2_blocks*512)-($-$$)) db 0x0 	;; fill with 0's
