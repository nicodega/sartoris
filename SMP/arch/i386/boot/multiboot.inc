
;;
;; Nasm struct definitions for multiboot.
;;

struc multiboot_header
.magic resd 1					;; 0 u32 magic 
.flags resd 1					;; 4 u32 flags
.checksum resd 1				;; 8 u32 checksum  required 
.header_addr resd 1				;; 12 u32 header_addr if flags[16] is set 
.load_addr resd 1				;; 16  u32  load_addr  if flags[16] is set 
.load_end_addr resd 1           ;; 20  u32  load_end_addr  if flags[16] is set 
.bss_end_addr resd 1            ;; 24  u32  bss_end_addr  if flags[16] is set 
.entry_addr resd 1				;; 28  u32  entry_addr  if flags[16] is set 
.mode_type resd 1				;; 32  u32  mode_type  if flags[2] is set 
.width resd 1					;; 36  u32  width  if flags[2] is set 
.height resd 1					;; 40  u32  height  if flags[2] is set 
.depth resd 1					;; 44  u32  depth  if flags[2] is set 
endstruc

%define MULTIBOOTH_MAGIC 0x1BADB002
%define MULTIBOOTH_FLAG_PAGEALIGN   0x1		;; bit 0 is 1
%define MULTIBOOTH_FLAG_MEM         0x2		;; bit 1 is 1
%define MULTIBOOTH_FLAG_VIDEO       0x4		;; bit 3 is 1
%define MULTIBOOTH_FLAG_ADDRINH     0x20000		;; bit 16 is 1

;;	 0       | flags             |    (required)
;;           +-------------------+
;;   4       | mem_lower         |    (present if flags[0] is set)
;;   8       | mem_upper         |    (present if flags[0] is set)
;;           +-------------------+
;;   12      | boot_device       |    (present if flags[1] is set)
;;           +-------------------+
;;   16      | cmdline           |    (present if flags[2] is set)
;;           +-------------------+
;;   20      | mods_count        |    (present if flags[3] is set)
;;   24      | mods_addr         |    (present if flags[3] is set)
;;           +-------------------+
;;   28 - 40 | syms              |    (present if flags[4] or
;;           |                   |                flags[5] is set)
;;           +-------------------+
;;   44      | mmap_length       |    (present if flags[6] is set)
;;   48      | mmap_addr         |    (present if flags[6] is set)
;;           +-------------------+
;;   52      | drives_length     |    (present if flags[7] is set)
;;   56      | drives_addr       |    (present if flags[7] is set)
;;           +-------------------+
;;   60      | config_table      |    (present if flags[8] is set)
;;           +-------------------+
;;   64      | boot_loader_name  |    (present if flags[9] is set)
;;           +-------------------+
;;   68      | apm_table         |    (present if flags[10] is set)
;;           +-------------------+
;;   72      | vbe_control_info  |    (present if flags[11] is set)
;;   76      | vbe_mode_info     |
;;   80      | vbe_mode          |
;;   82      | vbe_interface_seg |
;;   84      | vbe_interface_off |
;;   86      | vbe_interface_len |

%define MBOOTINFO_FLAGMEM 0x1
%define MBOOTINFO_FLAGMMAP 0x40

struc multiboot_info
.flags		resd 1 
.mem_lower	resd 1 
.mem_upper	resd 1 
.boot_device resd 1 
.cmdline resd 1 
.mods_count resd 1 
.mods_addr resd 1 
.syms resd 4 
.mmap_length resd 1 
.mmap_addr resd 1 
.drives_length resd 1 
.drive_addr resd 1 
.config_table resd 1 
.boot_loader_name resd 1 
.apm_table resd 1 
.vbe_control_info resd 1 
.vbe_mode_info resd 1 
.vbe_mode resw 1
.vbe_interface_seg resw 1 
.vbe_interface_off resw 1 
.vbe_interface_len resw 1  
endstruc

