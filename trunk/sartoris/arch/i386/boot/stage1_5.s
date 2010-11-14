
;;
;;	This is the default loading stage. It will use an encoded blocklist
;;	for loading the remains of stage 2.
;;	This stage could be replaced by other loaders in order to load stage 2
;;	from specific file systems.
;;  On this incarnation of the boot loader, we will support loading the kernel 
;;  on the 1MB mark and up. (this is to preserve the BIOS, in case we want to
;;  use the VESA BIOS extensions later on).
;;

bits 16	

;; each block list entry will have this format
struc blocklist_entry
.block resd 1
.length resw 1
endstruc

org 0x00200		;; this will tell nasm our current memory possition

%include "stages.inc"

%define loading_msg_len 7
%define boot_msg_len 23    

;; . <-- this is the entry point for stage 1.5
stage2_loader:
	;; we are now at the stage2seg16 segment
	;; ip = cs:0 = stage1_5seg16:0
	
	;; at this point we should have:
	;; ds points to bootseg16
	;; ds:si = segment:offset of disk address packet/drive geometry
	;; dl = drive num
	;; this must have been left this way by the stage 1
	;; We will use the stack left by stage 1.

	;; let es segment hold the boot 
	;; we will load the whole image

	;; initialize video for printing fancy text :)
	mov ax, 0x0003          ; set display to 80x25 16 color text mode
	int 0x10	

	;; preserve drive num
	mov bp, sp
	push dx
	sub sp, 2			    ;; bp + 2 will hold sector count
	mov word [bp - 4], 0		
	
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

	;; let us enter Unreal Mode!
	push ds
	push es
	cli
	
	lgdt [_gdt_pseudo_descr]
	
	;; enter protected mode
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	
	mov bx, 0x08          ; select descriptor 1
    mov  ds, bx            ; 8h = 1000b
    mov  es, bx
    
    mov eax, cr0
    and al, 0xFE           ; back to realmode
    mov cr0, eax           ; by toggling bit again

	pop es
	pop ds
	sti
	
	xor eax,eax
	
	;; now we should be able to use something like ds:eax or es:eax
	mov bx, ds
	mov es, bx
	
	pusha
	mov bp, boot_msg
        mov cx, boot_msg_len
        mov dx, 0x0200
        call print_msg          ; print 'bootstrap' message

	mov bp, loading_msg
        mov cx, loading_msg_len
        mov dx, 0x0400
        call print_msg          ; print 'bootstrap' message
	popa

	;; registers will be used this way on load_blocks
	;; dl = device num
	;; ds:si = stage 1 disk_address_packet/sectors
	;; ds:di = offset of current block list node

	;; read first list entry from ds:di
	mov di, block_list_start	
	
	;; set copy segment to nextstage_seg
	mov eax, dword [nextstage_addr]
	mov dword [nextcopy_addr], eax
	
load_blocks:
	
	;; check there's something to read
	cmp word [di + blocklist_entry.length], 0
	je near block_read_finished
	
	mov byte [tries], 0
	
	cmp byte [si - 1], 0    ; ds:[si-1] points to mode variable on stage1. If set to 1 it will use lba.
	je chs_mode
	
lba_mode:
	
	;; load logical sector start
	mov ebx, [di + blocklist_entry.block]

	;; the maximum is limited to 0x7f because of Phoenix EDD 
	xor	eax, eax
	mov al, 0x7f

	;; how many do we really want to read?
	cmp	[di + blocklist_entry.length], ax
	jg	lba_cont
	
	;; if less than, set to total
	mov ax, [di + blocklist_entry.length]

lba_cont:
	;; if we still are reading the stage2 loader
	;; compare with remaining
	cmp word [blocks_to_kernel], 0
	je lba_cont2

	cmp [blocks_to_kernel], ax
	jg  lba_cont2
	
	mov ax, [blocks_to_kernel]

lba_cont2:	
	;; add into logical sector start
	add [di + blocklist_entry.block], eax

	;; set up disk address packet 

	;; set the size and the reserved byte */
	mov word [si], 0x0010

	;; blocks 
	mov word [si + 2], ax		
	
	;; the absolute address (low 32 bits) 
	mov dword [si + 8], ebx
	
	pusha		;; preserve registers
	
	;; destination buffer segment 
	mov ax, 0x0
	mov word [si + 6], ax
	mov eax, stage1_5buf_addr
	sub eax, 0x7c00
	mov word [si + 4], ax
	
	xor	eax, eax
	mov [si + 10], eax		;; clear high bits of absolute address
	mov [si + 4], ax		;; clear offset for dest buffer
	
 ;; BIOS call "INT 0x13 Function 0x42" to read sectors from disk into memory
 ;;	Call with	ah = 0x42
 ;;				dl = drive number
 ;;				ds:si = segment:offset of disk address packet
 ;;	Return:
 ;;				al = 0x0 on success; err code on failure
 
	mov ah, 0x42
	int	0x13

	popa 
	
	jc near read_error

	jmp update_seg
	
chs_mode: 
	;; get next sector being read onto eax
	mov eax, [di + blocklist_entry.block]

translate_to_chs:
	;; translate stage 2 lba to chs
	
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

calc_sectors:
	xor eax, eax
	mov ax, [si]            ;; was 0x7f but it should use drive geometry for this and si points to sectors per track
	xor bx, bx
	mov bl, [si+10]
	sub ax, bx              ;; substract start sector
	inc ax                  ;; add one, because sectors are numbered from 1...N

	;; how many do we really want to read?
	cmp	[di + blocklist_entry.length], ax
	jg	calc_sectors_cont
	
	;; if less than, set to total
	mov ax, [di + blocklist_entry.length]
	
calc_sectors_cont:
	;; if we still are reading the stage2 loader
	;; compare with remaining
	cmp word [blocks_to_kernel], 0
	je load_track

	cmp [blocks_to_kernel], ax
	jg  load_track
	
	mov ax, [blocks_to_kernel]

load_track:
	;; add into logical sector start
	add [di + blocklist_entry.block], eax

	mov byte [tries], 0
	
	;; bios int 13 parameter translation
	mov ah, 2	; function: read
		
	mov cx, [si + 12]		; sector & track
	mov dl, cl
	shr cx, 2
	and cl, 0xc0
	or cl, [si + 10]
	mov ch, dl	

	mov dl, [bp - 2]			;; bp - 2 points to the drive num on stack
	mov dh, [si + 11]
	
	mov ebx, stage1_5buf_addr
	sub ebx, 0x7c00
	
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
;;			ah = 0x0 on success; err code on failure
	
	int 0x13
	popa
		
	xor ah, ah
		
	jnc update_seg
	cmp byte [tries], 3
bad_media:
	jae bad_media
	inc byte [tries]
	mov ah, 0x00
	mov dl, [bp - 2]
	int 0x13	;; reset
	jmp reload

update_seg:
	;; ax contains blocks read (no more than 0x7f on lba, and 0xff on chs)
	
	;; increment sectors read
	add [bp - 4], ax

	;; copy data from disk buffer to nextcopy_seg
	o32 pusha

	mov esi, stage1_5buf_addr
	mov edi, dword [nextcopy_addr]
	
	mov bx, 0
	mov es, bx
	mov ds, bx
		
	shl ax, 7		;; multiply by 512 / 4
	
	xor ecx, ecx
	mov cx, ax

	cld
	rep
	a32 movsd 		;; copy buffer using unreal mode

	mov bx, bootseg16
	mov es, bx
	mov ds, bx
	
	o32 popa

	;; subtract from blocklist entry length
	sub [di + blocklist_entry.length], ax

	cmp word [blocks_to_kernel], 0
	je update_seg_cont
	
	;; substract from blocks to kernel
	sub [blocks_to_kernel], ax
	
	cmp dword [blocks_to_kernel], 0
	jne update_seg_cont2
	
	;; we've just finished reading stage 2, set the new destination addr
	mov ebx, [kernel_img_addr]
	mov dword [nextcopy_addr], ebx

	jmp near update_seg_cont2
	
update_seg_cont:
	;; update nextcopy_segment
	mov ebx, dword [nextcopy_addr]
	shl eax, 9						;; multiply sectors by 512 to get the size in bytes
	add ebx, eax
	mov dword [nextcopy_addr], ebx
	
update_seg_cont2:
	call print_dot	

	jmp load_blocks

block_read_finished:
	
	;; substract 1 from blocklist len
	dec byte [block_list_len]
	
	;; see if next blocklist item must be read
	jz run		;; we finished
	
	add di, 6	;; move to next block list item
	jmp load_blocks
run:
	;; ok now we can run either an stage loader, or directly stage 2 (by default it'll be stage 2)
	mov ax, [nextstage_addr]
	shr ax, 4
	mov [nextstage_addr16+2], ax
	
	pop ax		;; set ax to stage 2 blocks
	pop dx		;; restore drive num
	
	jmp far [nextstage_addr16]
		
read_error:
	jmp $

print_msg:                     ; bp:   ptr to msg (in es)
       mov ax, 0x1301          ; cx:   msg len
       mov bx, 0x0007          ; dx:   row-col
       int 0x10
       ret

print_dot:
       mov ah, 0x0e            ; print a dot
       mov al, '.'
       xor bx, bx
       int 0x10
       ret
       
empty_kb_buf:			; this is the standard way, i belive.
	in al, 0x64
	and al, 0x02
	jnz empty_kb_buf
	ret

loading_msg:
	db 'Loading'
boot_msg:
	db 'Sartoris Bootloader 2.1'

	
gdt:	

dw 0, 0, 0, 0	; dummy descriptor

dw 0xffff ; limit=ffff
dw 0x0000 ; base_adress=0
dw 0x9200 ; p-flag=1, dpl=0, s-flag=1, type read/write
dw 0x00cf ; g-flag=1, d/b bit=1	, limit upper bits F

gdt_end:
	
_gdt_pseudo_descr:	
	dw gdt_end - gdt - 1     ; gdt limit
	dd ((stage1_5seg16 << 4) + (gdt - 0x200))

;; Data for stage 1.5
nextstage_addr16:		;; this will be used for the far jump
	dd 0

tries:
	db 0

nextcopy_addr:			;; this variable will hold the 32bit address where the next
	dd 0				;; sector will be placed upon reading from device.

nextstage_addr:
	dd stage2_addr		;; default to stage 2, if a file system loader should be used, this should be replaced by
						;; a different address, not colliding with stage 2 segment.

kernel_img_addr:		;; where the kernel image will be placed
	dd kernel_addr
	
blocks_to_kernel:		;; how many blocks will we copy, until we consider
	dw stage2_blocks	;; it a part of the kernel

update_addr:			;; if 1, stage 2 copy finished
	dw 0
	
;; let the block list be at a known location on the stage
times (903-($-$$)) db 0x00  	;;(we will leave space for 20 list nodes)

;; now block_list_len is always at location 391 on this stage
block_list_len:
		db 1			;; by default we will only have one entry
block_list_start:
		;; here we define our default entry
		;; starting at lba 3 with a length of (7*128)+2 512 bytes blocks.
		istruc blocklist_entry
		at blocklist_entry.block, dd 3			    ;; start coping at sector 3
		at blocklist_entry.length, dw (7*128)+2		;; copy the whole init image (if an os img is greater this should be changed)
		iend
		
;; complete with 0's 
times (1024-($-$$)) db 0x00

