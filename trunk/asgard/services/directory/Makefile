include ../../include/dep_rule.mk

CPPFLAGS += -I"../../include"

objects = directory_entry.o directory.o $(extobjects)

extobjects = ../../lib/structures/lptree.o ../../lib/structures/string.o ../../klib/kserv.o ../../lib/structures/avltree.o ../../lib/m_malloc.o ../../lib/structures/list.o ../../lib/critical_section.o ../../lib/int.o ../../lib/scheduler.o ../../lib/debug.o ../../drivers/screen/screen.o 

dependencies = directory.d

all : directory.img 

directory.img : $(objects) 
	$(LD) -T ../../tools/linkscript --Map directory.map -o directory.elf.img $(objects)
	../../tools/sld directory.elf.img directory.img
	#rm directory.elf.img
	

.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.map zeros

include ../../include/dep_attach.mk

