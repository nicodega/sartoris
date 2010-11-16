objects = $(extobjects) jedi.o
extobjects = ../../lib/init.o ../../klib/kserv-user.o ../../lib/scheduler.o ../../lib/printf.o

all : jedi.raw


jedi.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o jedi.raw $(objects)

.PHONY : clean
clean :
	rm -f *.o *~ jedi.raw
