objects = hello.o $(extobjects)
extobjects = ../../klib/kserv-user.o ../../lib/scheduler.o  ../../lib/malloc.o

all : hello.raw


hello.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o hello.raw $(objects)

.PHONY : clean
clean :
	rm -f *.o *~ hello.raw
