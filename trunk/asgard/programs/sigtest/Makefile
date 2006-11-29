objects = $(extobjects) sigtest.o
extobjects = ../../lib/stdlib.o ../../lib/psignals.o


all : sigtest.raw


sigtest.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map sigtest.map -o sigtest.elf.raw $(objects)
	../../tools/sld sigtest.elf.raw sigtest.raw
	#rm sigtest.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
