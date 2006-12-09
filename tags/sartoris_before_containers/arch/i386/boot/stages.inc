
;;
;;	This file contains defines used in both stage 1 and stage 2 loaders.
;;

;; real-mode segments 
%define bootseg16		0x07c0	; BIOS shall drop us there
%define	stage1_5seg16		0x0800	; stage 1.5 block will be copied here (this has to be within the range of the boot seg)
%define stage1_5bufseg16	0x1000	; 0x10000 to 0x20000 will be used as a buffer for reading from devices (buffer must be physicaly aligned on 64k boundaries for DMA transfers)
%define	stage2seg16		0x2000	; stage 2 will be copied here
%define stckseg16		0x9000	; stack for real mode operation

%define mbootinfseg16		0x0800	; where we will place Multiboot info on stage 2 until it's copied to it's location

%define mmap_offset		0xFF	; this ofset is within mbootinfseg16 segment

;; 32 bits physical address
%define mbootinfaddr		(mbootinfseg16*0x10)	; Physical address for multiboot
%define	stage2addr		0x20000	; stage 2 physical address
