objects = $(extobjects) cpptest.o

extobjects = ../../lib/stdlibcpp.o

all : cpptest.raw

cpptest.raw : $(objects)
	$(LD) -T ../../tools/linkscript --Map cpptest.map -o cpptest.elf.raw $(objects)
	../../tools/sld cpptest.elf.raw cpptest.raw
	rm cpptest.elf.raw

.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw zeros
