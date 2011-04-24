objects = $(extobjects) cat.o
extobjects = ../../lib/stdlib.o

all : cat.raw

cat.raw : $(objects) $(extobjects)
	$(LD) -T ../../tools/linkscript --Map cat.map -o cat.elf.raw $(objects)
	../../tools/sld cat.elf.raw cat.raw
	rm cat.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
