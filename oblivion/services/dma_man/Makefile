CFLAGS += -I"../../include"
objects = dma_man.o $(extobjects)
extobjects = ../../drivers/screen/screen.o ../../klib/kserv.o ../../drivers/dma/dma.o ../../lib/malloc.o

all : dma_man.img 

dma_man.img : dma_man.o
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N \
        --Map dma_man.map -o dma_man.img $(objects)
	./fill_dma.pl

.PHONY : clean
clean :
	rm -f *~
	rm -f *.o
	rm -f *.img *.map
