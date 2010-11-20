CFLAGS += -I"../../include"

objects = console.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/int.o ../../drivers/keyboard/keyb.o \
../../drivers/screen/screen.o ../../lib/print.o

all : console.img 

.PHONY : console.img
console.img : $(objects) 
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 \
--Map console.map -N -o console.img $(objects)
	./fill.pl


# depenencies
console.o : console.c csl_internals.h

.PHONY : clean
clean :
	rm -f *~ zeros console.map
	rm -f *.o
	rm -f *.img
