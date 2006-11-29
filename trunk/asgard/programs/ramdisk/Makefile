objects = $(extobjects) ramdisk.o 
extobjects = ../../lib/stdlib.o


all : ramdisk.raw


ramdisk.raw : $(objects)
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 --Map ramdisk.map -N \
-o ramdisk.raw $(objects)
	../../tools/fill ramdisk.raw -m 0x1000

.PHONY : clean
clean :
	rm -f *.o *~ ramdisk.raw
