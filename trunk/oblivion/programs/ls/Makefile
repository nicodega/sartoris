objects = ls.o $(extobjects)
extobjects = ../../klib/kserv.o ../../lib/scheduler.o


all : ls.raw

ls.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o ls.raw $(objects)

clean : 
	rm -f *.o *~ ls.raw
