
bits 32

;; an implementation for 64bit (long long) arithmetic
;; on IA32 architecture (this was just done in order to be able to use)
;; 64 bit artihmetics on the ofs service. 
;; NOTE: arithmetics code was extracted from and AMD example


global __ashldi3
global __ashrdi3
global __divdi3
global __moddi3
global __muldi3
global __negdi2
global __udivdi3
global __umoddi3


;; negation
;; __negdi2 (DWtype a)
__negdi2:
	mov  edx, [esp+20]   	; a_hi 
	mov  eax, [esp+16]   	; a_lo 
	not edx 
	neg eax 
	sbb edx, -1   ; Fix: Increment high word if low word was 0. 
	ret

;; multiplication
;; __muldi3 (DWtype u, DWtype v)
__muldi3:
	mov edx, [esp+8]    ; multiplicand_hi 
	mov ecx, [esp+16]   ; multiplier_hi 
   	or  edx, ecx        ; One operand >= 2^32? 
   	mov edx, [esp+12]   ; multiplier_lo 
   	mov eax, [esp+4]    ; multiplicand_lo 
   	jnz twomul          ; Yes, need two multiplies. 
   	mul edx             ; multiplicand_lo * multiplier_lo 
   	ret                 ; Done, return to caller. 
twomul: 
   	imul edx, [esp+8]         ; p3_lo = multiplicand_hi * multiplier_lo 
   	imul ecx, eax             ; p2_lo = multiplier_hi * multiplicand_lo 
   	add  ecx, edx             ; p2_lo + p3_lo 
   	mul  dword [esp+12]   ; p1 = multiplicand_lo * multiplier_lo 
   	add  edx, ecx             ; p1 + p2_lo + p3_lo = result in EDX:EAX 
   	ret                       ; Done, return to caller. 

;; divide two signed 64 bit numbers
;;__divdi3 (DWtype u, DWtype v)
__divdi3:
   push ebx    ; Save EBX as per calling convention. 
   push esi    ; Save ESI as per calling convention. 
   push edi    ; Save EDI as per calling convention. 
   mov  ecx, [esp+28]   ; divisor_hi 
   mov  ebx, [esp+24]   ; divisor_lo 
   mov  edx, [esp+20]   ; dividend_hi 
   mov  eax, [esp+16]   ; dividend_lo 
   mov  esi, ecx        ; divisor_hi 
   xor  esi, edx        ; divisor_hi ^ dividend_hi  
   sar  esi, 31         ; (quotient < 0) ? -1 : 0 
   mov  edi, edx        ; dividend_hi 
   sar  edi, 31         ; (dividend < 0) ? -1 : 0 
   xor  eax, edi        ; If (dividend < 0), 
   xor  edx, edi        ;  compute 1's complement of dividend. 
   sub  eax, edi        ; If (dividend < 0), 
   sbb  edx, edi        ;  compute 2's complement of dividend. 
   mov  edi, ecx        ; divisor_hi 
   sar  edi, 31         ; (divisor < 0) ? -1 : 0 
   xor  ebx, edi        ; If (divisor < 0), 
   xor  ecx, edi        ;  compute 1's complement of divisor. 
   sub  ebx, edi        ; If (divisor < 0), 
   sbb  ecx, edi        ;  compute 2's complement of divisor. 
   jnz  big_divisor     ; divisor > 2^32 - 1 
   cmp  edx, ebx        ; Only one division needed (ECX = 0)? 
   jae  two_divs        ; Need two divisions. 
   div  ebx             ; EAX = quotient_lo 
   mov  edx, ecx        ; EDX = quotient_hi = 0 (quotient in EDX:EAX) 
   xor  eax, esi        ; If (quotient < 0), 
   xor  edx, esi        ;  compute 1's complement of result. 
   sub  eax, esi        ; If (quotient < 0), 
   sbb  edx, esi        ;  compute 2's complement of result. 
   pop  edi             ; Restore EDI as per calling convention. 
   pop  esi             ; Restore ESI as per calling convention. 
   pop  ebx             ; Restore EBX as per calling convention. 
   ret                  ; Done, return to caller. 
two_divs: 
   mov  ecx, eax     ; Save dividend_lo in ECX. 
   mov  eax, edx     ; Get dividend_hi. 
   xor  edx, edx     ; Zero-extend it into EDX:EAX. 
   div  ebx          ; quotient_hi in EAX 
   xchg eax, ecx     ; ECX = quotient_hi, EAX = dividend_lo 
   div  ebx          ; EAX = quotient_lo 
   mov  edx, ecx     ; EDX = quotient_hi (quotient in EDX:EAX) 
   jmp  make_sign   ; Make quotient signed. 
big_divisor: 
   sub  esp, 12             ; Create three local variables. 
   mov  [esp], eax          ; dividend_lo 
   mov  [esp+4], ebx        ; divisor_lo 
   mov  [esp+8], edx        ; dividend_hi 
   mov  edi, ecx            ; Save divisor_hi. 
   shr  edx, 1              ; Shift both 
   rcr  eax, 1              ;  divisor and 
   ror  edi, 1              ;  and dividend 
   rcr  ebx, 1              ;  right by 1 bit. 
   bsr  ecx, ecx            ; ECX = number of remaining shifts 
   shrd ebx, edi, cl        ; Scale down divisor and 
   shrd eax, edx, cl        ;  dividend such that divisor is 
   shr  edx, cl             ;  less than 2^32 (that is, fits in EBX). 
   rol  edi, 1              ; Restore original divisor_hi. 
   div  ebx                 ; Compute quotient. 
   mov  ebx, [esp]          ; dividend_lo 
   mov  ecx, eax            ; Save quotient. 
   imul edi, eax            ; quotient * divisor high word (low only) 
   mul  dword [esp+4]   ; quotient * divisor low word 
   add  edx, edi            ; EDX:EAX = quotient * divisor 
   sub  ebx, eax            ; dividend_lo - (quot.*divisor)_lo 
   mov  eax, ecx            ; Get quotient. 
   mov  ecx, [esp+8]        ; dividend_hi 
   sbb  ecx, edx            ; Subtract (divisor * quot.) from dividend 
   sbb  eax, 0              ; Adjust quotient if remainder is negative. 
   xor  edx, edx            ; Clear high word of quotient. 
   add  esp, 12             ; Remove local variables. 
make_sign: 
   xor eax, esi   ; If (quotient < 0), 
   xor edx, esi   ;  compute 1's complement of result. 
   sub eax, esi   ; If (quotient < 0), 
   sbb edx, esi   ;  compute 2's complement of result. 
   pop edi        ; Restore EDI as per calling convention. 
   pop esi        ; Restore ESI as per calling convention. 
   pop ebx        ; Restore EBX as per calling convention. 
   ret            ; Done, return to caller. 

;; calculate reminder for signed
;;__moddi3 (DWtype u, DWtype v)
__moddi3:
   push ebx               ; Save EBX as per calling convention. 
   push esi               ; Save ESI as per calling convention. 
   push edi               ; Save EDI as per calling convention. 
   mov  ecx, [esp+28]     ; divisor-hi 
   mov  ebx, [esp+24]     ; divisor-lo 
   mov  edx, [esp+20]     ; dividend-hi 
   mov  eax, [esp+16]     ; dividend-lo 
   mov  esi, edx          ; sign(remainder) == sign(dividend) 
   sar  esi, 31           ; (remainder < 0) ? -1 : 0 
   mov  edi, edx          ; dividend-hi 
   sar  edi, 31           ; (dividend < 0) ? -1 : 0 
   xor  eax, edi          ; If (dividend < 0), 
   xor  edx, edi          ;  compute 1's complement of dividend. 
   sub  eax, edi          ; If (dividend < 0), 
   sbb  edx, edi          ;  compute 2's complement of dividend. 
   mov  edi, ecx          ; divisor-hi 
   sar  edi, 31           ; (divisor < 0) ? -1 : 0 
   xor  ebx, edi          ; If (divisor < 0), 
   xor  ecx, edi          ;  compute 1's complement of divisor. 
   sub  ebx, edi          ; If (divisor < 0), 
   sbb  ecx, edi          ;  compute 2's complement of divisor. 
   jnz  sr_big_divisor    ; divisor > 2^32 - 1 
   cmp  edx, ebx          ; Only one division needed (ECX = 0)? 
   jae  sr_two_divs       ; No, need two divisions. 
   div  ebx               ; EAX = quotient_lo 
   mov  eax, edx          ; EAX = remainder_lo 
   mov  edx, ecx          ; EDX = remainder_lo = 0 
   xor  eax, esi          ; If (remainder < 0), 
   xor  edx, esi          ;  compute 1's complement of result. 
   sub  eax, esi          ; If (remainder < 0), 
   sbb  edx, esi          ;  compute 2's complement of result. 
   pop  edi               ; Restore EDI as per calling convention. 
   pop  esi               ; Restore ESI as per calling convention. 
   pop  ebx               ; Restore EBX as per calling convention. 
   ret                    ; Done, return to caller. 
sr_two_divs: 
   mov ecx, eax        ; Save dividend_lo in ECX. 
   mov eax, edx        ; Get dividend_hi. 
   xor edx, edx        ; Zero-extend it into EDX:EAX. 
   div ebx             ; EAX = quotient_hi, EDX = intermediate remainder 
   mov eax, ecx        ; EAX = dividend_lo 
   div  ebx            ; EAX = quotient_lo 
   mov  eax, edx       ; remainder_lo 
   xor  edx, edx       ; remainder_hi = 0 
   jmp  sr_makesign    ; Make remainder signed. 
sr_big_divisor: 
   sub  esp, 16             ; Create three local variables. 
   mov  [esp], eax          ; dividend_lo              
   mov  [esp+4], ebx        ; divisor_lo              
   mov  [esp+8], edx        ; dividend_hi 
   mov  [esp+12], ecx       ; divisor_hi 
   mov  edi, ecx            ; Save divisor_hi. 
   shr  edx, 1              ; Shift both 
   rcr  eax, 1              ;  divisor and 
   ror  edi, 1              ;  and dividend 
   rcr  ebx, 1              ;  right by 1 bit. 
   bsr  ecx, ecx            ; ECX = number of remaining shifts 
   shrd ebx, edi, cl        ; Scale down divisor and 
   shrd eax, edx, cl        ;  dividend such that divisor is 
   shr  edx, cl             ;  less than 2^32 (that is, fits in EBX). 
   rol  edi, 1              ; Restore original divisor_hi. 
   div  ebx                 ; Compute quotient. 
   mov  ebx, [esp]          ; dividend_lo 
   mov  ecx, eax            ; Save quotient. 
   imul edi, eax            ; quotient * divisor high word (low only) 
   mul  DWORD [esp+4]   ; quotient * divisor low word 
   add  edx, edi            ; EDX:EAX = quotient * divisor 
   sub  ebx, eax            ; dividend_lo - (quot.*divisor)_lo 
   mov  ecx, [esp+8]        ; dividend_hi 
   sbb  ecx, edx            ; Subtract divisor * quot. from dividend. 
   sbb  eax, eax            ; remainder < 0 ? 0xffffffff : 0 
   mov  edx, [esp+12]       ; divisor_hi 
   and  edx, eax            ; remainder < 0 ? divisor_hi : 0 
   and  eax, [esp+4]        ; remainder < 0 ? divisor_lo : 0 
   add  eax, ebx            ; remainder_lo 
   add  edx, ecx            ; remainder_hi 
   add  esp, 16             ; Remove local variables. 
sr_makesign: 
   xor eax, esi   ; If (remainder < 0), 
   xor edx, esi   ;  compute 1's complement of result. 
   sub eax, esi   ; If (remainder < 0), 
  sbb edx, esi   ;  compute 2's complement of result. 
   pop edi        ; Restore EDI as per calling convention. 
   pop esi        ; Restore ESI as per calling convention. 
   pop ebx        ; Restore EBX as per calling convention. 
   ret            ; Done, return to caller. 


;; unsigned reminder
;; __umoddi3 (UDWtype u, UDWtype v)
__umoddi3:
   push ebx              ; Save EBX as per calling convention. 
   mov  ecx, [esp+20]    ; divisor_hi 
   mov  ebx, [esp+16]    ; divisor_lo 
   mov  edx, [esp+12]    ; dividend_hi 
   mov  eax, [esp+8]     ; dividend_lo 
   test ecx, ecx         ; divisor > 2^32 - 1? 
   jnz  r_big_divisor    ; Yes, divisor > 32^32 - 1. 
   cmp  edx, ebx         ; Only one division needed (ECX = 0)? 
   jae  r_two_divs       ; Need two divisions. 
   div  ebx              ; EAX = quotient_lo 
   mov  eax, edx         ; EAX = remainder_lo 
   mov  edx, ecx         ; EDX = remainder_hi = 0 
   pop  ebx              ; Restore EBX per calling convention. 
   ret                   ; Done, return to caller. 
r_two_divs: 
   mov ecx, eax   ; Save dividend_lo in ECX. 
   mov eax, edx   ; Get dividend_hi. 
   xor edx, edx   ; Zero-extend it into EDX:EAX. 
   div ebx        ; EAX = quotient_hi, EDX = intermediate remainder 
   mov eax, ecx   ; EAX = dividend_lo 
   div ebx        ; EAX = quotient_lo 
   mov eax, edx   ; EAX = remainder_lo 
   xor edx, edx   ; EDX = remainder_hi = 0 
   pop ebx        ; Restore EBX as per calling convention. 
   ret            ; Done, return to caller. 
r_big_divisor: 
   push edi                  ; Save EDI as per calling convention. 
   mov  edi, ecx             ; Save divisor_hi. 
   shr  edx, 1               ; Shift both divisor and dividend right 
   rcr  eax, 1               ;  by 1 bit. 
   ror  edi, 1 
   rcr  ebx, 1 
   bsr  ecx, ecx             ; ECX = number of remaining shifts 
   shrd ebx, edi, cl         ; Scale down divisor and dividend such 
   shrd eax, edx, cl         ;  that divisor is less than 2^32 
   shr  edx, cl              ;  (that is, it fits in EBX). 
   rol  edi, 1               ; Restore original divisor (EDI:ESI). 
   div  ebx                  ; Compute quotient. 
   mov  ebx, [esp+12]        ; dividend low word 
   mov  ecx, eax             ; Save quotient. 
   imul edi, eax             ; quotient * divisor high word (low only) 
   mul  DWORD [esp+20]   ; quotient * divisor low word 
   add  edx, edi             ; EDX:EAX = quotient * divisor 
   sub  ebx, eax             ; dividend_lo - (quot.*divisor)_lo 
   mov  ecx, [esp+16]        ; dividend_hi 
   mov  eax, [esp+20]        ; divisor_lo 
   sbb  ecx, edx             ; Subtract divisor * quot. from dividend. 
   sbb  edx, edx             ; (remainder < 0) ? 0xFFFFFFFF : 0 
   and  eax, edx             ; (remainder < 0) ? divisor_lo : 0 
   and  edx, [esp+24]        ; (remainder < 0) ? divisor_hi : 0 
   add  eax, ebx             ; remainder += (remainder < 0) ? divisor : 0 
   pop  edi                  ; Restore EDI as per calling convention. 
   pop  ebx                  ; Restore EBX as per calling convention. 
   ret                       ; Done, return to caller. 

;; unsigned division
;;__udivdi3 (UDWtype n, UDWtype d)
__udivdi3:
   push ebx             ; Save EBX as per calling convention. 
   mov  ecx, [esp+20]   ; divisor_hi 
   mov  ebx, [esp+16]   ; divisor_lo 
   mov  edx, [esp+12]   ; dividend_hi 
   mov  eax, [esp+8]    ; dividend_lo 
   test ecx, ecx        ; divisor > (2^32 - 1)? 
   jnz  ubig_divisor     ; Yes, divisor > 2^32 - 1. 
   cmp  edx, ebx        ; Only one division needed (ECX = 0)? 
   jae  utwo_divs        ; Need two divisions. 
   div  ebx             ; EAX = quotient_lo 
   mov  edx, ecx        ; EDX = quotient_hi = 0 (quotient in EDX:EAX) 
   pop  ebx             ; Restore EBX as per calling convention. 
   ret                  ; Done, return to caller. 
utwo_divs: 
   mov  ecx, eax   ; Save dividend_lo in ECX. 
   mov  eax, edx   ; Get dividend_hi. 
   xor  edx, edx   ; Zero-extend it into EDX:EAX. 
   div  ebx        ; quotient_hi in EAX 
   xchg eax, ecx   ; ECX = quotient_hi, EAX = dividend_lo 
   div  ebx        ; EAX = quotient_lo 
   mov  edx, ecx   ; EDX = quotient_hi (quotient in EDX:EAX) 
   pop  ebx        ; Restore EBX as per calling convention. 
   ret             ; Done, return to caller. 
ubig_divisor: 
   push edi                  ; Save EDI as per calling convention. 
   mov  edi, ecx             ; Save divisor_hi. 
   shr  edx, 1               ; Shift both divisor and dividend right 
   rcr  eax, 1               ;  by 1 bit. 
   ror  edi, 1 
   rcr  ebx, 1 
   bsr  ecx, ecx             ; ECX = number of remaining shifts 
   shrd ebx, edi, cl         ; Scale down divisor and dividend 
   shrd eax, edx, cl         ;  such that divisor is less than 
   shr  edx, cl              ;  2^32 (that is, it fits in EBX). 
   rol  edi, 1               ; Restore original divisor_hi. 
   div  ebx                  ; Compute quotient. 
   mov  ebx, [esp+12]        ; dividend_lo 
   mov  ecx, eax             ; Save quotient. 
   imul edi, eax             ; quotient * divisor high word (low only) 
   mul  dword [esp+20]   ; quotient * divisor low word 
   add  edx, edi             ; EDX:EAX = quotient * divisor 
   sub  ebx, eax             ; dividend_lo - (quot.*divisor)_lo 
   mov  eax, ecx             ; Get quotient. 
   mov  ecx, [esp+16]        ; dividend_hi 
   sbb  ecx, edx             ; Subtract (divisor * quot.) from dividend. 
   sbb  eax, 0               ; Adjust quotient if remainder negative. 
   xor  edx, edx             ; Clear high word of quot. (EAX<=FFFFFFFFh). 
   pop  edi                  ; Restore EDI as per calling convention. 
   pop  ebx                  ; Restore EBX as per calling convention. 
   ret                       ; Done, return to caller. 

