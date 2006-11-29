all : misbehav.raw

objects = $(extobjects) misbehav.o
extobjects = ../../lib/stdlib.o

misbehav.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map misbehav.map -o misbehav.elf.raw $(objects)
	../../tools/sld misbehav.elf.raw misbehav.raw
	#rm misbehav.elf.raw

clean : 
	rm -f *.o *~ *.raw *.map
