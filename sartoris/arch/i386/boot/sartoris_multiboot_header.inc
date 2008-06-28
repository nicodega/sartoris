
%include "multiboot.inc"

;;
;; This is the sartoris multiboot header.
;; This header will be placed at the begining of the os image.
;;

;; checksum = 0xFFFFFFFF & ~(flags + magic)

%define calc_checksum(flags) (0xFFFFFFFF & ~((flags) + MULTIBOOTH_MAGIC) + 1)

%define total_64kb_chunks 7
%define img_sectors (total_64kb_chunks * 128) 
%define img_size (img_sectors * 512)
%define entry_addr (image_phys_pos+0x30)

%define boot_info_size 0x58				;; 88 bytes

%define image_phys_pos 0x102000		;; we intend our kernel image to be placed here

sartoris_multiboot_header:
istruc multiboot_header
at multiboot_header.magic,		dd MULTIBOOTH_MAGIC
at multiboot_header.flags,		dd MULTIBOOTH_FLAG_ADDRINH | MULTIBOOTH_FLAG_MEM	;; our img will be in binary form, and mem info is required
at multiboot_header.checksum,	dd calc_checksum(MULTIBOOTH_FLAG_ADDRINH | MULTIBOOTH_FLAG_MEM)
at multiboot_header.header_addr, dd image_phys_pos	;; kernel image along with init must be placed here
at multiboot_header.load_addr,	dd image_phys_pos	;; The offset in the OS image file at which to start loading is defined by the offset 
															;; at which the header was found, minus (header_addr - load_addr)
at multiboot_header.load_end_addr,	dd 0x0		;; If this field is zero, the boot loader assumes that the text and data 
															;; segments occupy the whole OS image file. 
at multiboot_header.bss_end_addr,	dd 0x0		;; If this field is zero, the boot loader assumes that no bss segment is present. 
at multiboot_header.entry_addr,		dd entry_addr
;; Next is not required for we will initialize without vbe
;;at multiboot_header.mode_type,		dd
;;at multiboot_header.width,			dd
;;at multiboot_header.height,			dd
;;at multiboot_header.depth,			dd
iend
