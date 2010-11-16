CPPFLAGS += -I"../../include"

objects = pmanager.o $(extobjects)
extobjects = ../../klib/kserv.o ../../lib/int.o ../../lib/reboot.o \
../../drivers/screen/screen.o ../../drivers/pit/pit.o

all : pmanager.img 

.PHONY : pmanager.img
pmanager.img : pmanager.o 
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N \
--Map pmanager.map -o pmanager.img $(objects)
	./fill.pl

.PHONY : clean
clean :
	rm -f *~
	rm -f *.o
	rm -f *.img
	rm -f *.map zeros
