# this file sets up the assemble/compile/link environment

#CC = CC
CFLAGS = -O2 -D FPU_MMX
#LD = ld
AS = nasm
ASFLAGS = -f elf -d FPU_MMX

#CC = /home/guch/lcc/build/lcc
#CC = /usr/local/gcc-2.95.3/bin/gcc
#CC = ~/osdev/support/gcc-3.0.1/bin/gcc