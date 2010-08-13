# this file sets up the assemble/compile/link environment

CC = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-gcc
LD = /opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-ld
CFLAGS = -O2 -D FPU_MMX -D _METRICS_ -D PAGING
AS = nasm
ASFLAGS = -f elf -d FPU_MMX -D _METRICS_ -D PAGING
