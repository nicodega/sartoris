include ../../include/dep_rule.mk

CFLAGS += -I"../../include" -fPIC

objects = start.o ld.o ldio.o  $(extobjects)

extobjects = ../../klib/kserv-user.o ../../lib/scheduler.o ../../lib/debug.o ../../drivers/screen/screen.o

dependencies = ld.d ldio.d

all : ld.img 

.PHONY : ld.img
ld.img : $(objects) 
	$(LD) -T ./linkscript --Map ld.map -o ld.elf.img $(objects)
	../../tools/sld ld.elf.img ld.img
	
.PHONY : clean
clean :
	rm -f *~ *.o *.img *.d *.map zeros
include ../../include/dep_attach.mk

