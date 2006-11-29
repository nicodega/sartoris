objects = $(extobjects) fdisk.o
extobjects = ../../lib/stdlib.o


all : fdisk.raw


fdisk.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map fdisk.map -o fdisk.elf.raw $(objects)
	../../tools/sld fdisk.elf.raw fdisk.raw
	rm fdisk.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
