# this file sets up the assemble/compile/link environment

CC = /usr/i686-pc-elf/bin/i686-elf-gcc
LD = /usr/i686-pc-elf/bin/i686-elf-ld
CFLAGS = -O2 -D FPU_MMX -D _METRICS_ -D PAGING
AS = nasm
ASFLAGS = -f elf -d FPU_MMX -D _METRICS_ -D PAGING

