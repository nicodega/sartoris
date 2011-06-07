objects = $(extobjects) stest.o
extobjects = ../../lib/stdlib.o

CFLAGS += -I"../../include" -L../../lib

all : stest.raw

stest.o: stest.c 
	$(CC) $(CFLAGS) -c -o stest.o stest.c 

stest.raw : $(objects)
	$(LD) -nostdlib -T ../../tools/linkscript2 $(objects)  --dynamic-linker /usr/lib/ld.so --library-path=../../lib --library=stest --Map stest.map -o stest.raw 
	
.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
