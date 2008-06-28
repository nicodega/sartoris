;;
;;	THIS FILE HAS BEEN REPLACED BY A 3 STAGE LOADER.
;;	IT'S STILL HERE BECAUSE THE LOADER HASN'T BEEN 
;;	TESTED ENOUGH
;;

;; 
;; Ok This is an attempt to split the boot loading process.
;; This stage will load the kernel image. And also will move the first 
;; sector of the image (boot1.s sector) to the execution address of the bootloader
;;

bits 16	

;; real-mode segments 
%define bootseg16	0x07c0	; BIOS shall drop us there
%define	execseg16	0x6000	; and we'll find our way up here
%define mbrseg16	0x6400	; in case we need to load the mbr
%define stckseg16	0x9000	; stack for real mode operation
%define loadseg16	0x1000	; we will load the image here initially

%define kernel_address 0x00000000
%define init_address   0x00800000
%define stack_address  0x000a0000 - 0x4	; was 0x00300000 

;; note: if execseg16 is changed, make sure to update the _gdt_pseudo_descr
;;       to point to the new GDT in flat mode. santiago.

%define total_64kb_chunks 5

%define img_sectors (total_64kb_chunks * 128) 

%define img_size (img_sectors * 512)
	
%define mbr_part_offset 0x1be
%define mbr_part_size 0x10
%define mbr_last_entry (mbr_part_offset + mbr_part_size * 3)	

%define part_chs_offset 1
%define part_type_offset 4

%define sartoris_magic 0xd0


;; . <- this is the entry point

	mov ax, bootseg16
	mov ds, ax
	xor si, si
	
	les di, [exec_seg]
	
	;; ds:si -> 0x7c00   (initseg)
	;; es:di -> 0x60000  (execseg)
	
	mov cx, 0x100
	cld
	rep
	movsw

	;; we moved the bootsector to execseg
	
	jmp execseg16:go	
go:

	lss sp, [stack_seg]	; set up a stack
	
	lds si, [exec_seg]	; load ds with execseg
	
	mov [drive_num], dl
	
	cmp dl, 0x80
	jb boot_media_known ;; this is the easy case: booting from a floppy 
	
	;; first things first. get BIOS disk geometry.

	mov ah, 8
	int 0x13
	and cx, 0x3f
	mov [spt], cx
	dec cx			; we won't read the bootsector
	mov [dsec_to_read], cx
	shr dx, 8
	mov [head_num], dx	; zero-based
	
	;; ok, we must read the MBR to memory.

	mov ax, 0x0201		; int 0x13 function 2, read 1 sector

	mov bx, mbrseg16
	mov es, bx
	xor bx, bx

	mov cx, 0x0001		; track 0, sector 1
	
	mov dl, [drive_num]
	mov dh, 0		; head 0
	int 0x13

	;; now we scan the partition table

	cmp word [es:510], 0xaa55	; check for sartoris signature 
nombr:
	jne nombr
	
	mov bx, mbr_part_offset
scan_pt:
	cmp byte [es:bx+part_type_offset], sartoris_magic
	je match
	
	cmp bx, mbr_last_entry
not_found:
	je not_found

	add bx, mbr_part_size
	jmp scan_pt
	
match:			
	xor ax, ax
	mov al, [es:bx + part_chs_offset]
	mov [head], ax
	mov al, [es:bx + part_chs_offset+1]
	mov cl, al
	shl cx, 2
	and al, 0x3f
	inc ax
	mov [sector], ax
	mov cl, [es:bx + part_chs_offset + 2]
	mov [track], cx
	
boot_media_known:	

	;; load next 512 bytes of the bootloader
	;; on 0x6200

	les bx, [bootpos]

	jmp load_track 		; skip initialization

load_img:

	xor ax, ax
	mov [loadingboot], ax
	mov ax, [dsec_to_read]
	mov [sec_to_read], ax

	;; this is the loader routine.
	;; we will load entire tracks (when possible).

	les bx, [pos]
	
load_track:

	mov byte [tries], 0
	
	mov ah, 2		; function: read
	
	mov al, [sec_to_read]	; sectors to read
		
	mov cx, [track]		; sector & track
	mov dl, cl
	shr cx, 2
	and cl, 0xc0
	or cl, [sector]
	mov ch, dl	

	mov dl, [drive_num]
	mov dh, [head]

reload:
	pusha
	int 0x13
	popa
	
	jnc load_ok
	cmp byte [tries], 3
bad_media:
	jae bad_media
	inc byte [tries]
	jmp reload
	
load_ok:
	cmp [loadingboot], word 0	; see if we were loading the boot record second sector
	jne load_img

	;; set up next load
	xor ah, ah		; ax = last [sec_to_read]
	mov dx, ax
	shl dx, 9
	add bx, dx
	jnz no_seg_update


;; invariant property:	either the last read didn't cross a 64kb boundary,
;; or it ended exactly in a 64kb boundary. 

;; another way to see it:
;; if a read is not an entire track, then it ends exactly in a 64kb boundary.
		
	sub word [chunks], 1
	;; finished loading, jump to the 
	jz finished
	
	mov dx, es
	add dx, 0x1000
	mov es, dx

no_seg_update:		

	;; if [sector]+[sec_to_read]-1 = [spt] then we must start a track read
	;; otherwise, we must just finish a track fraction
	;; (by invariant property, this track crosses a 64kb boundary)
	
	mov cx, ax
	add cx, [sector]
	dec cx
	cmp cx, [spt]
	je no_finish_partial
	
	;; finish partial read:	issue the last fraction of the track.
	
	mov cx, [spt]
	sub cx, ax
	mov [sec_to_read], cx
	inc ax
	mov [sector], ax

	jmp load_track

finished:
	jmp load_done

no_finish_partial:

	mov dx, [spt]
	mov [sec_to_read], dx
	
	mov word [sector], 1
	
	mov dx, [head]
	inc dx
	
	cmp dx, [head_num]
	jbe no_track_update
	
	xor dx, dx
	inc word [track]

no_track_update:
	
	mov [head], dx
	
	mov dx, bx		; dx = pointer in current 64kb chunk
	mov cx, [spt]
	shl cx, 9		; cx = bytes of next read
	add dx, cx		; dx = dx + cx
	
	jnc no_start_partial	; if there was an overflow...
				; dx contains how many bytes we crossed
	sub cx, dx		
	shr cx, 9		; how many sectors can be read
	mov [sec_to_read], cx

no_start_partial:

	jmp load_track

;; loading variables

bootpos:
	dw 0
	dw (execseg16 + 0x20)

dsec_to_read:
	dw 17 ; default value 17 is for floppy

loadingboot:
	dw 1
	
chunks:
	dw total_64kb_chunks
	
;; boot media information. the default is a 1.44 inch boot
;; floppy:  18 sectors per track, 2 heads.
	
drive_num:			; BIOS number
	db 0
	
;; geometry: 
spt:
	dw 18
head_num:			; zero-based
	dw 1

;; reading position 
track:
	dw 0
head:
	dw 0
sector:
	dw 2

sec_to_read:
	dw 1; first we will load the next boot sector
	
pos:
	dw 0
	dw loadseg16
		
tries:
	db 0

;; some useful far pointers.
	
stack_seg:
	dw 0x1000-0x4
	dw stckseg16

exec_seg:
	dw 0
	dw execseg16

;; complete with 0's and the signature
times (510-($-$$)) db 0x00
	dw 0xaa55		; signature

;; Second boot sector
load_done:

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
		
;; switch to p-mode

	cli
	
	lgdt [_gdt_pseudo_descr]
	lidt [_idt_pseudo_descr]
	
	mov eax, cr0
	or eax, 1
	mov cr0, eax

	jmp dword 0x0008:(execseg16*0x10 + go32)		; mama!
go32:		
bits 32

	mov ax, 0x0010
	mov ds, ax 
	mov es, ax
	mov ss, ax

	;; move the kernel down to 0x0
	mov ecx, ((img_size - 512) >> 2)
	mov esi, loadseg16 * 0x10 + 0x200
	mov edi, kernel_address
	cld
	rep
	movsd

	;; place init img where it should be
	mov ecx, ( (img_sectors - (kern_sectors + 1)) * 512) >> 2
	mov esi, kern_sectors*512 + kernel_address
	mov edi, init_address
	rep
	movsd
	
	;; ok, we have copied the kernel down, and the init service up.
	;; let's jump to the kernel initialization routines.

	mov esp, stack_address	

	jmp dword 0x8:0x0	; kernel init running!

bits 16

empty_kb_buf:			; this is the standard way, i belive.
	in al, 0x64
	and al, 0x02
	jnz empty_kb_buf
	ret


;; this system tables are temporary, the kernel will
;; update them later

gdt:	

dw 0, 0, 0, 0	; dummy descriptor
	
dw 0x0fff ; limit=4095	(4096*4096=16777216, 16Mb)
dw 0x0000 ; base_adress=0
dw 0x9a00 ; p-flag=1, dpl=0, s-flag=1 (non-system segment), type code exec/read
dw 0x00c0 ; g-flag=1, d/b bit=1	(limit mult by 4096, default 32-bit opsize)

dw 0x0fff ; limit=4095
dw 0x0000 ; base_adress=0
dw 0x9200 ; p-flag=1, dpl=0, s-flag=1, type read/write
dw 0x00c0 ; g-flag=1, d/b bit=1	
	
_gdt_pseudo_descr:	
	dw 0x0030		; gdt_limit=48 (3 descriptors)
	dw gdt, 0x6
	
_idt_pseudo_descr:	
	dw 0x0000		; interrupts will be desabled until 
	dw 0x0000, 0x0000	; the real system tables are ready.
	
times (1024-($-$$)) db 0x0 	;; fill with 0's
