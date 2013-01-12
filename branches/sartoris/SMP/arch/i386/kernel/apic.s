
	;; Sartoris i386 interrupt support
	;;
	;; Copyright (C) 2002, Santiago Bazerque and Nicolas de Galarreta
	;; 
	;;  sbazerqu@dc.uba.ar
	;;  nicodega@gmail.com
	
bits 32

global init_ioapic
global ioapic_max_ints
global hard_ints_count
global ioapic_map_int
global ioapic_set_int_state

global apic_eoi
global start_apic

%define APIC_ID    0x20
%define APIC_VER   0x30
%define APIC_TPR   0x80
%define APIC_APR   0x90
%define APIC_PPR   0xA0
%define APIC_EOI   0xB0
%define APIC_LDR   0xD0		; logical destination register
%define APIC_DFR   0xE0		; destination format
%define APIC_SIVT  0xF0		; spurious interrupt vector 
%define APIC_ISR   0x100
%define APIC_TMR   0x180
%define APIC_IRR   0x200
%define APIC_ESR   0x280
%define APIC_ICR   0x300
%define APIC_ICR32    0x310
%define APIC_LVT_TR   0x320
%define APIC_LVT_TSR  0x330
%define APIC_LVT_PMCR 0x340
%define APIC_LVT_LINT0  0x350
%define APIC_LVT_LINT1  0x360
%define APIC_LVT_ERR    0x370
%define APIC_ICR   0x380
%define APIC_CCR   0x390
%define APIC_DCR   0x3E0

%define APIC_DISABLE_IOAPIC_EOI 0x1000

global apic_eoi_broadcast

apic_eoi_broadcast:
    dd 1

;; send a command to the IO apic
;; ioapic_write base offset value
%macro ioapic_write 3
	mov [%1], dword %2
	mov [%1+0x10], dword %3
%endmacro

%macro ioapic_read 2
	mov [%1], dword %2
	mov eax, dword [%1+0x10]
%endmacro

%define IOAPIC_MASKED 0x10000
%define IOAPIC_NOTMASKED 0xFFFEFFFF
%define APIC_BROADCAST_SUPPORT 0x1000000

hard_ints_count:
    dd 0

;; this function will start the specified IOAPIC, masking all interrupts
;; start_apic(byte apicid, void *address, unsigned int global_vector_start)
init_ioapic:
	push ebp
	mov ebp, esp
	push ebx
	push esi
	push edi

	mov esi, [ebp+16]	;; esi has vector start
	mov edx, [ebp+12]	;; edx has ioapic base

	; get apic ints
	ioapic_read edx, 0x01
    shr eax, 16
    and eax, 0xFF

	mov ecx, eax        ;; ebx has apic ints count
	mov edi, 0  		;; ioapic int number * 2

	; mask all interrupts and send them to the specified apic
init_ioapic_maskint:
	; build on eax the value of the low part
    mov eax, edi
    shr eax, 1                      ; now eax has ioapic int number
    add eax, esi                    ; add global vector number
	add eax, 32
    or eax, IOAPIC_MASKED
    mov ebx, edi
    add ebx, 0x10
	ioapic_write edx, ebx, eax
	
	mov eax, dword [ebp+8]         ; apic id
	shl eax, 12
    mov ebx, edi
    add ebx, 0x11
	ioapic_write edx, ebx, eax
    add edi, 2
    inc dword [hard_ints_count]
	dec ecx
	jnz init_ioapic_maskint
    	
	pop edi
	pop esi
	pop ebx
	pop ebp
	ret

;; unsigned char ioapic_max_ints(void *address)
ioapic_max_ints:
	push ebp
	mov ebp, esp

	mov ecx, [ebp+8]	;; ecx has ioapic base

	ioapic_read ecx, 0x01
	
	shr eax, 16
	and eax, 0xFF

	pop ebp
	ret
        
;; void ioapic_set_int_state(void *address, int intnum, int state);
ioapic_set_int_state:
	push ebp
	mov ebp, esp
    
	mov edx, [ebp+12]	;; edx has int num
	mov ecx, [ebp+8]	;; ebx has base address

    shl edx, 1
    add edx, 0x10       ;; now it has the offset of the low part
        
	; read apic config
	ioapic_read ecx, edx

    ; set mask
    cmp dword [ebp+16], 0x1              ; int state
    je ioapic_set_int_state_enable
    or eax, IOAPIC_MASKED	
    jmp ioapic_set_int_state_cont

ioapic_set_int_state_enable:
    and eax, IOAPIC_NOTMASKED

ioapic_set_int_state_cont:
    ioapic_write ecx, edx, eax

    pop ebp
	ret

;; void ioapic_map_int(void *address, int intnum, unsigned char vector, int polarity, int tgmode, int intstate, unsigned char apicid)
ioapic_map_int:
	push ebp
	mov ebp, esp
    push ebx

    ; build an entry with:
    ; IntVector = vector
    ; Delivery Mode = 00
    ; Dest mode = 0
    ; Polarity = polarity
    ; Mask = if intstate = 1 will disable

    mov eax, [ebp+16]       ; vector
    and eax, 0x000000FF
    mov edx, [ebp+20]       ; polarity
    shl edx, 13
    or eax, edx
    mov edx, [ebp+24]        ; trig
    shl edx, 15
    or eax, edx
    cmp dword [ebp+28], 0x1
    je ioapic_map_int_enabled
    or eax, 0x10000         ; disable the int
ioapic_map_int_enabled:
    mov ecx, [ebp+8]       ; base addr
    mov edx, [ebp+12]       ; int num
    shl edx, 1              ; multiply by 2
    add edx, 0x11           ; add first offset + 1 for second register
			
    mov ebx, dword [ebp+32]         ; apic id
	shl ebx, 12

	ioapic_write ecx, edx, ebx

    dec edx

	ioapic_write ecx, edx, eax
	
    pop ebx
	pop ebp
	ret

;; this function will start the local processor APIC
;; start_apic(void *address)
start_apic:
	push ebp
	mov ebp, esp

	mov edx, [ebp+8]

	; disable 8259
	mov al, 0xFF
	out 0x21, al
	mov al, 0xFF
	out 0xA1, al

	; axcept all ints (even processor ints)
	mov dword [edx + APIC_TPR], 0x0
	; disable timer ints
	mov dword [edx + APIC_LVT_TR], 0x10000
	; disable performance counter interrupts
	mov dword [edx + APIC_LVT_PMCR], 0x10000
	; enable normal external interrupts
	mov dword [edx + APIC_LVT_LINT0], 0x08700 
	; enable normal NMI processing
	mov dword [edx + APIC_LVT_LINT1], 0x00400
	; disable error interrupts
	mov dword [edx + APIC_LVT_ERR], 0x10000 

    ; read APIC_VER bit 24 and check if we can disable EOI broadcast
    ;;mov eax, dword [edx + APIC_VER]
    ;;and eax, APIC_BROADCAST_SUPPORT
    ;;jz start_apic_no_br_support
    ;mov [apic_eoi_broadcast], 0
	;; program the spurious int vector to FF and enable the apic
	;;mov dword [edx + APIC_SIVT], (APIC_DISABLE_IOAPIC_EOI | 0x1FF)
	;;pop ebp
	;;ret
start_apic_no_br_support:
    mov dword [edx + APIC_SIVT], (0x1FE)
    mov dword [edx + APIC_EOI], (0x0)       ; in case any ints where triggered before (all ints are masked now)   
	pop ebp
	ret

;; REMEMBER TO cli/sti while writing ICR

extern localApicAddress

;; void apic_eoi();
apic_eoi:
    xor eax, eax
    mov edx, [localApicAddress]
    mov dword [edx+APIC_EOI], eax
    ret

