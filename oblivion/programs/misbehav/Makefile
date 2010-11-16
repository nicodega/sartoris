objects = misbehav.o $(extobjects)
extobjects = ../../klib/kserv.o


all : misbehav.raw

misbehav.raw : misbehav.o
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o misbehav.raw $(objects)

clean : 
	rm -f *.o *~ misbehav.raw misbehav.s
