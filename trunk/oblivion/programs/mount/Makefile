objects = mount.o $(extobjects)
extobjects = ../../klib/kserv.o

all : mount.raw

mount.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o mount.raw $(objects)

.PHONY : clean
clean :
	rm -f *.o *~ mount.raw
