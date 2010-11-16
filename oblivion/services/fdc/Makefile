CFLAGS += -I"../../include"
objects = fdc.o $(extobjects)
extobjects = ../../drivers/screen/screen.o ../../klib/kserv.o ../../drivers/floppy/floppy.o ../../drivers/rtc/rtc.o

all : fdc.img 

fdc.img : fdc.o
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N \
        --Map fdc.map -o fdc.img $(objects)
	./fill_fdc.pl

.PHONY : clean
clean :
	rm -f *~
	rm -f *.o
	rm -f *.img
	rm -f *.map zeros
