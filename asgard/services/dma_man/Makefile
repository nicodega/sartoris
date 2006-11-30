include ../../include/dep_rule.mk

CFLAGS += -I"../../include"

objects = dma_entry.o dma_man.o $(extobjects)

extobjects = ../../drivers/screen/screen.o ../../klib/kserv.o ../../drivers/dma/dma.o ../../lib/pm_malloc.o ../../lib/structures/list.o ../../lib/int.o ../../lib/scheduler.o ../../lib/debug.o

dependencies = dma_entry.d dma_man.d

all : dma_man.img 

dma_man.img : dma_man.o dma_entry.o
	$(LD) -T ../../tools/linkscript --Map dma_man.map -o dma_man.elf.img $(objects)
	../../tools/sld dma_man.elf.img dma_man.img
	#rm dma_man.elf.img

.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.map

include ../../include/dep_attach.mk

