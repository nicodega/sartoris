
bits 32

global _int_start

extern int_handler

_int_start:
    sub esp, 4          ;; the channel is on the stack
    jmp int_handler
