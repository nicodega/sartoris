objects = $(extobjects) fmaptst.o
extobjects = ../../lib/stdlib.o

CFLAGS += -I"../../include" -L../../lib

all : fmaptst.raw

fmaptst.o: fmaptst.c 
	$(CC) $(CFLAGS) -c -o fmaptst.o fmaptst.c 

fmaptst.raw : $(objects)
	$(LD) -nostdlib -T ../../tools/linkscript $(objects)  --Map fmaptst.map -o fmaptst.raw 
	
.PHONY : clean
clean :
	rm -f *.o *~ *.map *.raw
