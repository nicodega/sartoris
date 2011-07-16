
;; note: if execseg16 is changed, make sure to update the _gdt_pseudo_descr
;;       to point to the new GDT in flat mode. santiago.


;;
;; This will conform sartoris Stage 2 boot sector. This stage is 
;; (Almost, because we won't support video modes changing) Multiboot conforming.
;;

%include "multiboot.inc"
%include "stages.inc"

%define stage2_blocks 2

bits 16

stage2_entry:
	;; stage 2 has been loaded completely in memory
	;; NOTE: The kernel image is now not next to this image.
	
	;; stage 1.5 loader must have left:
	;; Unreal Mode on.
	;; ds/es: 0x7c0
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
	sub cx, stage2_blocks
	
	;; ds will point to stage2seg16
	mov ax, stage2seg16
	mov ds, ax
	
	;; bx will point to the Multiboot header (located at the begining of the kernel image)
	xor ebx, ebx
	mov ebx, (kernel_addr - stage2_addr)		;; now the kernel is loaded here... yay!
	
	;; check first value is the magic number
	a32 cmp dword [ebx], MULTIBOOTH_MAGIC
	jne near error
	
	;; use checksum for checking flags and magic number
	a32 mov eax, [ebx]
	a32 add eax, [ebx + 4]
	a32 add eax, [ebx + 8]
	
	cmp eax, 0
	jne near error
	
	;; setup things so the image can be run

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

	jmp dword 0x0008:(stage2_addr + go32)		; mama!
go32:		
bits 32

	mov ax, 0x0010
	mov ds, ax 
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	
;; EAX contains the magic value 0x2BADB002
;; EBX contains the 32-bit physical address of the Multiboot information structure provided by the boot loader. 
;; CS is a 32-bit read/execute code segment with an ofeset of 0 and a limit of 0xFFFFFFFF. The exact value is undefined. 
;; DS,ES,es,GS,SS are 32-bit read/write data segment with an offset of 0 and a limit of 0xFFFFFFFF. The exact values are all undefined. 
;;
;; A20 gate is enabled. 
;; CR0 Bit 31 (PG) is cleared. Bit 0 (PE) is set. Other bits are all undefined. 
;; EFLAGS 
;;		Bit 17 (VM) is cleared. Bit 9 (IF) is cleared. Other bits are all undefined. 
;;		All other processor registers and flag bits are undefined. 

	mov ebx, kernel_addr
	o32 mov edx, [ebx + multiboot_header.entry_addr]

	mov eax, MULTIBOOTH_MAGIC
	mov ebx, mbootinfaddr
	
	jmp edx		; this should jump to 0x8:edx

bits 16

error:
	jmp $

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
	dd (stage2_addr + gdt)
	
_idt_pseudo_descr:	
	dw 0x0000		; interrupts will be desabled until 
	dd 0x00000000	; the real system tables are ready.

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

	;; if this is not the first attempt (meaning ebp != 0)
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

times ((stage2_blocks*512)-($-$$)) db 0x0 	;; fill with 0's
