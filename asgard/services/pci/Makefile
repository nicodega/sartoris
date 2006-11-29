include ../../include/dep_rule.mk

CPPFLAGS += -I"../../include" 

objects = helpers.o pci_srv.o pci_commands.o pcienum.o  $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/scheduler.o ../../lib/sprintf.o ../../lib/debug.o ../../drivers/screen/screen.o ../../lib/m_malloc.o ../../lib/int.o ../../lib/critical_section.o ../../lib/structures/list.o ../../lib/structures/stdlibsim.o 

dependencies = helpers.d pci_srv.d pci_commands.d pcienum.d 

all : pci.img 

.PHONY : pci.img
pci.img : $(objects) 
	$(LD) -T ../../tools/linkscript --Map pci.map -o pci.elf.img $(objects)
	../../tools/sld pci.elf.img pci.img
	#rm pci.elf.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.img *.d *.map zeros
include ../../include/dep_attach.mk

