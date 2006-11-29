objects = $(extobjects) jedi.o
extobjects = ../../lib/stdlib.o


all : jedi.raw


jedi.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map jedi.map -o jedi.elf.raw $(objects)
	../../tools/sld jedi.elf.raw jedi.raw
	#rm jedi.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw zeros
