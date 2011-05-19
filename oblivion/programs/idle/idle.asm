global thexchg

extern run_thread

bits 32
die:
    jmp die
thexchg: xchg bx,bx
die3:
    push 1
    call run_thread     ;; reschedule
    add esp, 4
    jmp die3
