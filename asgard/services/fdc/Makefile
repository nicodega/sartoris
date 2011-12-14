include ../../include/dep_rule.mk

CFLAGS += -I"../../include"

objects = fdc_entry.o fdc.o $(extobjects)

extobjects = ../../drivers/screen/screen.o ../../lib/scheduler.o ../../lib/structures/stdlibsim.o ../../lib/int.o ../../klib/kserv.o ../../drivers/floppy/floppy.o ../../drivers/rtc/rtc.o ../../lib/debug.o \
../../lib/wait_msg_sync.o

dependencies = fdc.d

#please leave this line not using the template
#for gcc optimizations breaks timeout waiting loops
all : fdc.img 

fdc.img : $(objects)
	$(LD) -T ../../tools/linkscript --Map fdc.map -o fdc.elf.img $(objects)
	../../tools/sld fdc.elf.img fdc.img
	#rm fdc.elf.img

.PHONY : fdc.o
fdc.o : fdc.c
	$(CC) -O0 -c -ffreestanding -nostdinc -nostdlib -nostartfiles -I"../../include" -s fdc.c -o fdc.o

.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.map zeros

include ../../include/dep_attach.mk

