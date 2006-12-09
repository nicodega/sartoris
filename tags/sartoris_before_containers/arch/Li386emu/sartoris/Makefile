CC = gcc
CPPFLAGS += -I"../../../include" -I"../include" -I.
CFLAGS += -O2 -ffreestanding -nostdlib
AS = nasm
ASFLAGS = -f elf

%.d : %.c
	@echo making deps for $<
	@gcc -E -MD $(CPPFLAGS) $< > /dev/null 

objects = ../emulator/emulator.o ../kernel/kernel-emu.o ../../../mk/kimg.o

sartoris : $(objects) init.img
	gcc $(objects) -o sartoris

init.img : init.o ../lib/emulib.o
	ld -oformat binary -Ttext 0 -o init.img ../lib/emulib.o init.o

init.o : init.c ../lib/emulib.o 
	gcc -c $(CPPFLAGS)  init.c

.PHONY : clean
clean :
	rm -f sartoris core

dep :
