objects = dbg.o $(extobjects)
extobjects = ../../klib/kserv-user.o ../../lib/scheduler.o ../../lib/printf.o

all : dbg.raw

dbg.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N -o dbg.raw $(objects)

.PHONY : clean
clean :
	rm -f *.o *~ dbg.raw
