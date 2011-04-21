
;; 
;; This will be the Stage 1 bootloader. It will be exactly 512 bytes in size.
;; Things will be left so that our stage 1.5 boot loader can load itself.
;; This boot loader stage, won't do much. It's a simple stage 1 loader.
;; This loader will only load first sector of the stage 1.5 loader.
;;

bits 16	

%include "stages.inc"

;; . <- this is the entry point

	;; this is because of the BIOS parameter block
	jmp afterBPB
	
times (0xB - ($-$$)) db 0x00 ;; pad until BPB

mode:
	db	0
disk_address_packet:	
sectors:
	dd	0
heads:
	dd	0
cylinders:
	dw	0
sector_start:
	db	0
head_start:
	db	0
cylinder_start:
	dw	0
	
times ((24 + 0xB) - ($-$$)) db 0x00

;; End of BIOS parameter block.

;; stage 2 (or 1.5) first sector lba. This is a known position parameter. 
;; will be at pos: 0x23 on the MBR.
stage2_sector:
	dd 1

afterBPB:
	;; some bogus BIOS jump to 07C0:0000 instead of 0000:7C00
	jmp word 0:((bootseg16<<4)+go)
go:
	mov ax, bootseg16
	mov ds, ax
	xor si, si

	mov ax, stckseg16
	mov ss, ax
	mov ax, 0xEFFF - 0x4
	mov sp, ax

	mov [drive_num], dl		; get the drive number bios left for us
	
	;; do not probe LBA if the drive is a floppy 
	cmp dl, 0x80
	jb chs_mode
	
	;; check for lba mode
	mov	ah, 0x41
	mov	bx, 0x55aa
	int	0x13

	;; use chs mode if failed
	jc	chs_mode
	cmp bx,	0xaa55
	jne	chs_mode
	
	;; check if AH=0x42 is supported */
	and	cx, 1
	jz	chs_mode
lba_mode:

	;; set si to the disk address packet 
	mov	si, disk_address_packet

	;; set the mode to non-zero 
	mov	byte [mode], 1
	
	mov ebx, [stage2_sector]

	;; set the size and the reserved byte 
	mov word [si], 0x0010

	;; blocks 
	mov word [si + 2], stage1_5blocks	;; we will stage 1.5 blocks
	
	;; the absolute address (low 32 bits) 
	mov dword [si + 8], ebx
	
	;; destination buffer segment 
	mov word [si + 6], stage1_5seg16

	xor	eax, eax
	mov [si + 4], ax		;; clear the offset for the buffer address
	mov [si + 10], eax		;; clear high bits of absolute address
	

 ;; BIOS call "INT 0x13 Function 0x42" to read sectors from disk into memory
 ;;	Call with	ah = 0x42
 ;;				dl = drive number
 ;;				ds:si = segment:offset of disk address packet
 ;;	Return:
 ;;				al = 0x0 on success; err code on failure
 
	mov dl, [drive_num]
 
	mov ah, 0x42
	int	0x13

	;; LBA read is not supported, so fallback to CHS.  
	jc	chs_mode

	mov bx, stage1_5seg16
	jmp	load_done
	
chs_mode:
		
	;; first things first. get BIOS disk geometry.
	mov ah, 8
	int 0x13
	
	;; set mode to 0
	mov si, sectors
	
	mov byte [si - 1], 0
		
	;; save number of heads 
	xor	eax, eax
	mov	al, dh
	inc	ax
	mov	[si + 4], eax

	xor	dx, dx
	mov	dl, cl
	shl dx, 2
	mov al, ch
	mov ah, dh

	;; save number of cylinders
	inc	ax
	mov [si + 8], ax

	xor	ax, ax
	mov	al, dl
	shr al, 2

	;; save number of sectors 
	mov [si], eax

translate_to_chs:
	;; translate stage 2 lba to chs
	mov eax, [stage2_sector]

	xor	edx, edx

	;; divide by number of sectors 
	div dword [si]

	;; save sector start 
	inc dl						;; sectors goes from 1..N
	mov	[si + 10], dl

	xor	edx, edx	
	div dword [si + 4]			;; divide by number of heads

	;; save head start 
	mov [si + 11], dl

	;; save cylinder start 
	mov [si + 12], ax

load_track:

	mov byte [tries], 0
	
	mov ah, 2	; function: read
	
	mov al, stage1_5blocks	; sectors to read
		
	mov cx, [si + 12]		; sector & track
	mov dl, cl
	shr cx, 2
	and cl, 0xc0
	or cl, [si + 10]
	mov ch, dl	

	mov dl, [drive_num]
	mov dh, [si + 11]
	
	mov bx, stage1_5seg16
	mov es , bx
	xor bx, bx

reload:
	pusha
	
;; BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
;;	Call with	ah = 0x2
;;			al = number of sectors
;;			ch = cylinder
;;			cl = sector (bits 6-7 are high bits of "cylinder")
;;			dh = head
;;			dl = drive (0x80 for hard disk, 0x0 for floppy disk)
;;			es:bx = segment:offset of buffer
;;	Return:
;;			al = 0x0 on success; err code on failure
	
	int 0x13
	popa
	
	jnc load_ok
	cmp byte [tries], 3
	
bad_media:
	jae bad_media
	inc byte [tries]
	jmp reload
	
load_ok:
	
	mov bx, stage1_5seg16
	jmp	load_done


load_done:

	mov dl, [drive_num]
	mov cx, 1

	;; at this point we should have:
	;; ds:si = segment:offset of disk address packet/sectors
	;; dl = drive num
	;; cx = 1 (blocks loaded)

	;; exec stage 2
	jmp stage1_5seg16:0
	
;; Stage 1 data 

;; loading variables
		
drive_num:			; BIOS drive number
	db 0
tries:
	db 0

;; Fill with 0's until the partition table
times (446-($-$$)) db 0x00
	
;; complete with 0's and the signature
times (510-($-$$)) db 0x00
	dw 0xaa55		; signature
