objects = umount.o $(extobjects)
extobjects = ../../klib/kserv.o

all : umount.raw

umount.raw : $(objects)
	$(LD) --oformat binary --entry 0x0000000 --Ttext 0x00000000 -N \
-o umount.raw $(objects)

.PHONY : clean
clean : 
	rm -f *.o *~ umount.raw
