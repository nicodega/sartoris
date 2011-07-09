include ../../include/dep_rule.mk

CFLAGS += -I"../../include"
#CFLAGS =  -O0 -ffreestanding -nostdinc -nostdlib -nostartfiles 

objects = console_entry.o console.o mouse.o consolestddev.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/int.o ../../drivers/mouse/mouse.o ../../drivers/keyboard/keyb.o \
../../drivers/screen/screen.o ../../lib/structures/list.o \
../../lib/pmalloc.o ../../lib/scheduler.o ../../lib/debug.o

dependencies = console_entry.d console.d consolestddev.d mouse.d

all : console.img 

.PHONY : console.img
console.img : $(objects) 
	$(LD) -T ../../tools/linkscript --Map console.map -N -o console.elf.img $(objects)
	../../tools/sld console.elf.img console.img
	#rm console.elf.img

.PHONY : clean
clean :
	rm -f *.o *.img *.d *~ zeros console.map

include ../../include/dep_attach.mk





