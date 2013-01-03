/opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-gcc sizes.c -c -I"../include" -I"../arch/target/include" -o sizes.o -D __KERNEL__ -O2 -D FPU_MMX -D _METRICS_ -D PAGING -ffreestanding -nostdinc -nostdlib -nostartfiles 
/opt/crosstool/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-ld sizes.o --oformat binary -Ttext 0x0000000 -e 0x00000000 -Map sizes.map -o sizes
rm sizes
rm sizes.o