include ../../include/dep_rule.mk

CFLAGS += -I"../../include" 

objects = pipes_entry.o pipes_srv.o commands.o preadgen.o pwritegen.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/scheduler.o ../../lib/int.o ../../lib/debug.o ../../drivers/screen/screen.o ../../lib/pmalloc.o ../../lib/rawpiolib.o ../../lib/structures/avltree.o ../../lib/structures/list.o ../../lib/structures/stdlibsim.o ../../lib/structures/string.o

dependencies = pipes_entry.d pipes_srv.d commands.d preadgen.d pwritegen.d

all : pipes.img 

.PHONY : pipes.img
pipes.img : $(objects) 
	$(LD) -T ../../tools/linkscript --Map pipes.map -o pipes.elf.img $(objects)
	../../tools/sld pipes.elf.img pipes.img
	#rm pipes.elf.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.img *.d *.map zeros
include ../../include/dep_attach.mk

