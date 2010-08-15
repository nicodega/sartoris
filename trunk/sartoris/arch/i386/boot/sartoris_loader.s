
;;
;; This is the kernel loader. This loader is multiboot conforming, 
;; meaning it could be loaded either by our Stage 1 or by any Multiboot supporting
;; boot loader like Grub.
;;
;; This stage should leave the kernel at possition 0 on memory and the init image
;; at its defined position.
;;

;; First things first... the multiboot structure
;; is to be placed at the begining of the kernel img.
%include "sartoris_multiboot_header.inc"

;; Machine state when we are runned is:

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

%define multiboot_info_address   0x100000		;; we will place multiboot info here
%define kernel_address 0x00000000				;; we will place the kernel here
%define init_address   0x00800000				;; we will place the init image here
%define stack_address  0x000a0000 - 0x4			;; we will place the stack here

%define loader_sectors 1

%define realaddress(x) ((x) + (image_phys_pos))

bits 32

;; . <-- this is the entry point (And also this is possition 0 on the kernel img.)
_start: 
	;; check we where loaded by a multiboot loader
	cmp eax, MULTIBOOTH_MAGIC
	jne near die							;; if not loaded by a multiboot loader, die!

	;; - Even though the segment registers are set up as described above, the GDTR may be invalid, so the OS image must 
	;; not load any segment registers (even just reloading the same values!) until it sets up its own GDT. 
	;; - The OS image must leave interrupts disabled until it sets up its own IDT.
	
	;; load gdt and idt with our tables
	lgdt [realaddress(_gdt_pseudo_descr)]
	lidt [realaddress(_idt_pseudo_descr)]
	
	;; jump to cs:ep2
	jmp 0x8:(image_phys_pos + ep2)
ep2:

	;; now it's safe to load our segment registers :D
	mov ax, 0x0010
	mov ds, ax 
	mov es, ax
	mov ss, ax

	;; ebx contains the 32 bit physical address of the multiboot inf structure
	;; as our data segment base address is 0, it's ok to use that address as it
	;; was left by multiboot
	;; mov eax,[ebx + multiboot_info.xxxx]

	;; copy bootinfo struct
	mov ecx, ((boot_info_size) >> 2)
	mov esi, ebx
	mov edi, multiboot_info_address
	cld
	rep
	movsd

	;; copy mmap if there is one
	mov eax, [ebx + multiboot_info.flags]
	test eax, MBOOTINFO_FLAGMMAP
	jz nommap

	mov ebp, [ebx + multiboot_info.mmap_length]
	cmp ebp, 0
	je nommap

	;; copy mmap and update bootinfo pointer :)
	mov esi, [ebx + multiboot_info.mmap_addr]	 ;; source
	mov edi, multiboot_info_address + boot_info_size ;; dest

copymmap_entry:
	mov ecx, [esi]	;; get member size
	sub ebp, ecx
	
	cld
	rep
	movsb
	
	;; esi and edi are now at the begining of the next map item
	cmp ebp, 0
	jne copymmap_entry

	mov ebx, multiboot_info_address
	mov dword [ebx + multiboot_info.mmap_addr], (multiboot_info_address + boot_info_size)
nommap:
	;; move the kernel down to 0x0
	mov ecx, (((kern_sectors - loader_sectors) * 512) >> 2)		;; how many bytes will we copy? (we ignore loader sectors)
	mov esi, image_phys_pos+(loader_sectors*512)		;; where do we start? (this is the value in our 
								;; multiboot header + size of this loader)
	mov edi, kernel_address					;; destination
	cld
	rep
	movsd

	;; place init img where it should be
	mov ecx, [realaddress(img_size)]
	sub ecx, ((kern_sectors + loader_sectors) * 512)
	shr ecx, 2
	mov esi, image_phys_pos + (kern_sectors + loader_sectors)*512
	mov edi, init_address
	rep
	movsd
	
	;; ok, we have copied the kernel down, and the init service up.
	;; let's jump to the kernel initialization routines.

	mov esp, stack_address		;; don't forget to setup the stack 

	jmp dword 0x8:0x0	; kernel init running!
	
die:
	jmp $
	
	
;; gdt table we will load
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
	dw (gdt + (image_phys_pos & 0x0000FFFF)), (image_phys_pos >> 16)
	
_idt_pseudo_descr:	
	dw 0x0000           ; interrupts will be desabled until 
	dw 0x0000, 0x0000   ; the real system tables are ready.
	
	
times (508-($-$$)) db 0x0 	;; fill with 0's

img_size: dd (7*128*512)

