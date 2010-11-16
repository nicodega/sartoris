CPPFLAGS += -I"../include"

objects = init.o $(extobjects)
extobjects = ../klib/kserv.o ../drivers/screen/screen.o

all : init.img

init.o : init.c ../include/oblivion/layout.h

.PHONY : init.img
init.img : $(objects)
	$(LD) --oformat binary --Ttext 0x00000000 -e 0x000000000 -N  -Map init.map -o init.img $(objects)
	./fill_init.pl
	cat ../services/p-manager/pmanager.img >> init.img
	cat ../services/console/console.img >> init.img
	cat ../services/fdc/fdc.img >> init.img
	cat ../services/filesys/filesys.img >> init.img
	cat ../services/ramfs/ramfs.img >> init.img
	cat ../services/dma_man/dma_man.img >> init.img

.PHONY : clean
clean :
	rm -f *.o *~ init.img init.map zeros
