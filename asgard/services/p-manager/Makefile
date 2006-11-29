CPPFLAGS += -I"../../include"
include ../../include/dep_rule.mk

CFLAGS += -D PMAN_SIZE=`./pmansize.pl`

objects = pmanager_entry.o pmanager_init.o elf.o elffileio.o pman_debug.o pman_malloc.o init_helpers.o pmanager.o paging_fault_handler.o paging_swap.o paging_sched.o paging_steal.o paging_aging.o paging_init.o init_fs.o services_spawn.o page_list.o paging.o exceptions.o fileio.o commands.o signals.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/int.o ../../lib/reboot.o \
../../drivers/screen/screen.o ../../drivers/pit/pit.o \
../../lib/sprintf.o

dependencies = pmanager_entry.d paging_fault_handler.d paging_swap.d paging_sched.d paging_steal.d paging_aging.d paging_init.d pmanager_init.d services_spawn.d init_helpers.d pmanager.d init_fs.d page_list.d paging.d exceptions.d fileio.d commands.d elf.d elffileio.d pman_malloc.d signals.d
	
./sizecalc/%.o : %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -s $< -o $@

./sizecalc/%.o : %.s
	$(AS) $(ASFLAGS) $< -o $@


all : pmansize pmanager.img 
	$(MAKE) -C images
	cp ./pmanager.img ./pmanager.raw
	cat ./images/init_images.img >> pmanager.img

# For building pmanager img, it's necesary to know the size it'll
# have once compiled, for setting define PMAN_SIZE.
# what I'll do is compiling it with PMAN_SIZE = 0 and then
# stat the image and recompile.
.PHONY : sizecalc
sizecalc:
	mkdir -p ./sizecalc

SIZECALCOBJ = $(patsubst %.o, ./sizecalc/%.o, $(filter-out ../%, $(objects)) )  $(filter ../%, $(objects))  

.PHONY : pmansize
pmansize : sizecalc $(SIZECALCOBJ)
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N -o ./sizecalc/pmanager.img $(SIZECALCOBJ)  
	mv ./sizecalc/pmanager.img ./pmanager.img
	../../tools/fill pmanager.img -m 0x1000

.PHONY : pmanager.img
pmanager.img : $(objects)
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N --Map pmanager.map -o pmanager.img $(objects)
	../../tools/fill pmanager.img -m 0x1000

		
.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.raw *.map zeros
	rm -f ./sizecalc/*
	$(MAKE) -C images clean

include ../../include/dep_attach.mk

