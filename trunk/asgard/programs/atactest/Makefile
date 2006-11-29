objects = $(extobjects) atactest.o
extobjects = ../../lib/stdlib.o


all : atactest.raw


atactest.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map atactest.map -o atactest.elf.raw $(objects)
	../../tools/sld atactest.elf.raw atactest.raw
	#rm atactest.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
