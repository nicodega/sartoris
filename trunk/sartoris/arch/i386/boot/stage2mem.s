
;; note: if execseg16 is changed, make sure to update the _gdt_pseudo_descr
;;       to point to the new GDT in flat mode. santiago.


;;
;; This will conform sartoris Stage 2 boot sector. This stage is 
;; (Almost, because we won't support video modes changing) Multiboot conforming.
;;

%include "multiboot.inc"
%include "stages.inc"

%define stage_2_blocks 2
   
%define realaddress(x) (x + (stage2seg16*0x10-bootseg16*0x10))

bits 16

stage2_entry:

	;; stage 2 has been loaded completely in memory
	;; NOTE: On Sartoris Stage 2 is supposed to contain the init image.
	
	;; stage 1.5 loader must have left:
	;; ds:si = segment:ofeset of disk address packet/drive geometry
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
	
	;; ds will point to stage2seg16
	mov ax, stage2seg16
	mov ds, ax
	
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
	call get_mem_size

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
;; CS is a 32-bit read/execute code segment with an ofeset of 0 and a limit of 0xFFFFFFFF. The exact value is undefined. 
;; DS,ES,es,GS,SS are 32-bit read/write data segment with an ofeset of 0 and a limit of 0xFFFFFFFF. The exact values are all undefined. 
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

bits 16

;; This function will attempt getting memory size 
;; by using different methods
;; this function will not preserve: es, si, ax, bx, dx, cx
get_mem_size:
	
	;; set es:si to multiboot info structure possition
	mov ax, mbootinfseg16
	mov es, ax
	xor si, si

	;; set multiboot info flag
	xor eax, eax
	or eax, MBOOTINFO_FLAGMEM
	mov [es:si + multiboot_info.flags], eax


	mov dx, 0 ;; conventional
	call get_msize
	mov [es:si + multiboot_info.mem_lower], eax
	mov dx, 1 ;; extended
	call get_msize
	mov [es:si + multiboot_info.mem_upper], eax

	;; attempt getting an mmap
	mov dword [es:si + multiboot_info.mmap_length], 0									;; initial length
	mov [es:si + multiboot_info.mmap_addr], dword ((mbootinfseg16<<4) + mmap_offset)	;; physical address for mmap
	
	xor edi, edi
	mov di, mmap_offset
	xor ebx, ebx
	mov ebp, 0				;; ebp will hold length
	
getmmap_loop:
	call get_mmap
	cmp ebx, 0
	je getmmap_finished
	add edi, [es:di]		;; increment buffer possition
	jmp getmmap_loop
getmmap_finished:
	mov dword [es:si + multiboot_info.mmap_length], ebp 

	cmp ebp, 0
	je use_get_mem_2

	;; Set mmap flag
	or [es:si + multiboot_info.flags], dword MBOOTINFO_FLAGMMAP
	
	;; I will try to cap high mem using mem2 system, so let it go through 
	;; NOTE: I could also cap upper by going through the mmap,
	;; but I'll leave that to others :D

use_get_mem_2:
	;; attempt getting mem size using function 0xE801
	call get_mem2

	cmp eax, 0xffffffff
	je get_mem_finished

	;; set high mem
	and ebx, 0x0000ffff
	shl ebx, 6	;; convert to Kilobytes
	add eax, ebx
	
	mov [es:si + multiboot_info.mem_upper], eax

get_mem_finished:
	ret

;; Get mem size by int 12h or 15h
;; dx = 1 then ext, dx = 0 conv.
;; returned in eax
get_msize:
	xor eax,eax

	cmp dx, 1
	je get_msize_ext

	int 0x12

	jmp get_msize_done
get_msize_ext:

	mov ah, 0x88
	int 0x15	

get_msize_done:
	mov bx, ax
	xor eax,eax
	mov ax, bx

	ret

;; this function will call int 15 function 0xE820
;; parameters are:
;; ebx: continuation value or 0 for the begining of the map
;; es:di = buffer of 20 bytes
;; ebp: size of buffer read so far (initialy 0)
;; returns: 
;; ebx: continuation value
;; this function will not preserve ax, bx, cx, dx
get_mmap:
	mov eax, 0x0000e820
	mov edx, 0x534D4150 ;; 'SMAP'	
	mov ecx, 0x14
	;; ebx already has the cont value
	;; es:di already points at the buffer, but we will add size to it 
	;; because the BIOS won't return it
	mov dword [es:di], 0
	mov [es:di], dword 0x18    ;; set the size on the buffer
	add di, 4
	
	int 0x15

	;; if this is not the first attempt (meaning esp != 0)
	cmp ebp, 0
	jne get_mmap_not_first
		
	;; First time, perform common checks
	jc nommap

	cmp eax, 0x534D4150 ;; 'SMAP'	
	jne nommap

	cmp ecx, 0x14
	jne nommap 
	
	add ebp, 0x18	;; increment here for it's the first
	
	jmp mmapok
get_mmap_not_first:
	add ebp, 0x18		;; increment counter on 1
	
	;; if carry or ebx = 0, this is the last record
	jc nommap
	
	cmp ebx, 0x0
	je nommap
	
	cmp ecx, 0x14
	jne nommap

mmapok:
	sub di, 4	;; return buffer pointer as it was
	ret
nommap:
	mov ebx, 0x0
	sub di, 4	;; return buffer pointer as it was
	ret
	
;; if failed eax will be 0xffffffff, else 0x0000XXXX 
;; where XXXX has the lower part and bx the higher
get_mem2:
	mov eax, 0x0000E801
	int 0x15
	
	jc getmem2fail

	cmp ah, 0x86
	je getmem2fail

	cmp ax, 0
	jne getmem2cont

	mov ax, cx
	mov bx, dx
getmem2cont:
	mov cx, ax
	xor eax, eax
	mov ax, cx
	ret
getmem2fail:
	mov eax, 0xffffffff
	ret

times ((stage_2_blocks*512)-($-$$)) db 0x0 	;; fill with 0's
