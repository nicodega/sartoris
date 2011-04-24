objects = $(extobjects) more.o
extobjects = ../../lib/stdlib.o

all : more.raw

more.raw : $(objects) $(extobjects)
	$(LD) -T ../../tools/linkscript --Map more.map -o more.elf.raw $(objects)
	../../tools/sld more.elf.raw more.raw
	rm more.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
