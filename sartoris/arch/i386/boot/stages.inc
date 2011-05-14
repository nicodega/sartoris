
;;
;;	This file contains defines used in both stage 1 and stage 2 loaders.
;;

;; real-mode segments 
%define bootseg16           0x07c0  ; BIOS shall drop us there
%define	stage1_5seg16       0x07e0  ; stage 1.5 block will be copied here (this has to be within the range of the boot seg)
%define stckseg16           0x8000  ; stack for real mode operation

%define stage2seg16			0x820
%define mbootinfseg16       0x0900  ; where we will place Multiboot info on stage 2 until it's copied to it's location

%define mmap_offset	        0xFF    ; this ofset is within mbootinfseg16 segment

%define stage1_5blocks      0x2     ; how many block has stage 1.5
%define stage2_blocks       0x2     ; how many block has stage 2

;; 32 bits physical address (also for unreal mode)
%define	kernel_addr         0x210000              ; kernel will be copied here (not right on 2MB because multiboot will be placed there)
%define mbootinfaddr        (mbootinfseg16*0x10)  ; Physical address for multiboot
%define	stage2_addr         0x00008200            ; stage 2 physical address (must be below 1MB mark)
%define stage1_5buf_addr    0x10000  ; 0x10000 to 0x20000 will be used as a buffer for reading from devices (buffer must be physicaly aligned on 64k boundaries for DMA transfers)
