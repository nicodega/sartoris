
;;
;;	This is the default loading stage. It will use an encoded blocklist
;;	for loading the remains of stage 2.
;;	This stage could be replaced by other loaders in order to load stage 2
;;	from specific file systems.
;;

bits 16	

;; each block list entry will have this format
struc blocklist_entry
.block resd 1
.length resw 1
endstruc

%define realaddress(x) ((x) + (stage1_5seg16*0x10-bootseg16*0x10))

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
	;; this must have been left this way by the stage 1 (or 1.5)
	;; loader
	;; We will use the stack left by stage 1.

	;; let es hold the boot 
	;; we must load reamining bytes for the 2nd stage 
	
	;; registers will be used this way on load_blocks
	;; dl = device num
	;; ds:si = stage 1 disk_address_packet/sectors
	;; ds:di = offset of current block list node

	;; initialize video for printing fancy text xD
	mov ax, 0x0003          ; set display to 80x25 16 color text mode
	int 0x10	

	;; preserve drive num
	mov bp, sp
	push dx
	sub sp, 2			;; bp + 2 will hold sector count
	mov word [bp - 4], 0		
	
	pusha
	mov ax, ds
	mov es, ax
	mov bp, realaddress(boot_msg)
        mov cx, boot_msg_len
        mov dx, 0x0200
        call print_msg          ; print 'bootstrap' message

	mov bp, realaddress(loading_msg)
        mov cx, loading_msg_len
        mov dx, 0x0400
        call print_msg          ; print 'bootstrap' message
	popa 

	;; read first list entry from ds:di
	mov di, realaddress(block_list_start)	
	
	;; set copy segment to nextstage_seg
	mov ax, [realaddress(nextstage_seg)+2]
	mov [realaddress(nextcopy_seg)], ax
	
	
load_blocks:
	
	;; check there's something to read
	cmp word [di + blocklist_entry.length], 0
	je near block_read_finished
	
	mov byte [realaddress(tries)], 0
	
	cmp byte [si - 1], 0
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
	;; subtract from total 
	sub [di + blocklist_entry.length], ax

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
	mov ax, stage1_5bufseg16
	mov [si + 6], ax
	

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
	sub ax, bx		;; substract start sector
	inc ax			;; add one, because sectors are numbered from 1...N

	;; how many do we really want to read?
	cmp	[di + blocklist_entry.length], ax

	jg	load_track

	;; if less than, set to total
	mov ax, [di + blocklist_entry.length]

load_track:
	;; subtract from total 
	sub [di + blocklist_entry.length], ax

	;; add into logical sector start
	add [di + blocklist_entry.block], eax

	mov byte [realaddress(tries)], 0
	
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
	
	mov bx, stage1_5bufseg16
	mov es, bx
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
;;			ah = 0x0 on success; err code on failure
	
	int 0x13
	popa
		
	xor ah, ah
		
	jnc update_seg
	cmp byte [realaddress(tries)], 3
bad_media:
	jae bad_media
	inc byte [realaddress(tries)]
	mov ah, 0x00
	mov dl, [bp - 2]
	int 0x13	;; reset
	jmp reload

update_seg:
	;; ax contains blocks read (no more than 0x7f on lba, and 0xff on chs)

	;; increment sectors read
	add [bp - 4], ax

	;; copy data from disk buffer to nextcopy_seg
	pusha
	push ds

	mov bx, [realaddress(nextcopy_seg)]
	mov es, bx
	mov bx, stage1_5bufseg16
	mov ds, bx
	xor si, si
	xor di, di

	shl ax, 8		;; multiply by 512 / 2
	mov cx, ax

	cld
	rep
	movsw 	;; copy buffer

	pop ds
	popa

	;; update nextcopy_segment
	mov bx, [realaddress(nextcopy_seg)]
	shl ax, 5		
	add bx, ax
	mov [realaddress(nextcopy_seg)], bx

	call print_dot	

	jmp load_blocks

block_read_finished:
	
	;; substract 1 from blocklist len
	dec byte [realaddress(block_list_len)]
	
	;; see if next blocklist item must be read
	jz run		;; we finished
	
	add di, 6	;; move to next block list item
	jmp load_blocks
run:
	pop ax		;; set ax to stage 2 blocks
	pop dx		;; restore drive num
	
	;; ok now we can run either an stage loader, or directly stage 2 (by default it'll be stage 2)
	jmp far [realaddress(nextstage_seg)]
		
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

loading_msg:
	db 'Loading'
boot_msg:
	db 'Sartoris Bootloader 2.0'

;; Data for stage 1.5
tries:
	db 0
	
nextcopy_seg:
	dw 0

nextstage_seg:
	dw 0
	dw stage2seg16		;; default to stage 2, if a file system loader should be used, this should be replaced by
						;; a different address, not colliding with stage 2 segment.

;; let the block list be at a known location on the stage	

times (391-($-$$)) db 0x00  	;;(we will leave space for 20 list nodes)

;; now block_list_len is always at location 391 on this stage
block_list_len:
		db 1			;; by default we will only have one entry
block_list_start:
		;; here we define our default entry
		;; starting at lba 2 with a length of (7*128)+2 512 bytes blocks.
		istruc blocklist_entry
		at blocklist_entry.block, dd 2			;; start coping at sector 2
		at blocklist_entry.length, dw (7*128)+2		;; copy the whole init image (if an os img is greater this should be changed)
		iend
		
;; complete with 0's 
times (512-($-$$)) db 0x00
