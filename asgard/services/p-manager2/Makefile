CFLAGS += -I"../../include"
include ../../include/dep_rule.mk

CFLAGS += -D PMAN_SIZE=`./pmansize.pl`

objects = pman_entry.o pman_init_step2.o  command.o command_queue.o elf.o elf_io_callbacks.o exceptions.o helpers.o interrupts.o io.o kmalloc.o loader.o loader_init.o pman_main.o pman_print.o scheduler.o shutdown.o signals.o task.o thread.o vmm.o vmm_aging.o vmm_brk.o vmm_file_mmap.o vmm_page_stack.o vmm_pghandler.o vmm_phy_mmap.o vmm_regions.o vmm_shared_pages.o vmm_stealing.o vmm_swap.o vmm_swap_bitmap.o $(extobjects)

extobjects = ../../klib/kserv.o ../../lib/int.o ../../lib/reboot.o \
../../drivers/screen/screen.o ../../drivers/pit/pit.o \
../../lib/sprintf.o

dependencies = command.d command_queue.d elf.d elf_io_callbacks.d exceptions.d helpers.d interrupts.d io.d kmalloc.d loader.d loader_init.d pman_entry.d pman_init_step2.d pman_main.d pman_print.d scheduler.d shutdown.d signals.d task.d thread.d vmm.d vmm_aging.d vmm_brk.d vmm_file_mmap.d vmm_page_stack.d vmm_pghandler.d vmm_phy_mmap.d vmm_regions.d vmm_shared_pages.d vmm_stealing.d vmm_swap.d vmm_swap_bitmap.d 
	
./sizecalc/%.o : %.c
	$(CC) -c $(CFLAGS) $(CFLAGS) -s $< -o $@

./sizecalc/%.o : %.s
	$(AS) $(ASFLAGS) $< -o $@


all : pmansize pmanager2.img 
	$(MAKE) -C images
	cp ./pmanager2.img ./pmanager2.raw
	cat ./images/init_images.img >> pmanager2.img

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
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N -o ./sizecalc/pmanager2.img $(SIZECALCOBJ)  
	mv ./sizecalc/pmanager2.img ./pmanager2.img
	../../tools/fill pmanager2.img -m 0x1000

.PHONY : pmanager2.img
pmanager2.img : $(objects)
	$(LD) --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N --Map pmanager2.map -o pmanager2.img $(objects)
	../../tools/fill pmanager2.img -m 0x1000

		
.PHONY : clean
clean :
	rm -f *~ *.o *.d *.img *.raw *.map zeros
	rm -f ./sizecalc/*
	$(MAKE) -C images clean

include ../../include/dep_attach.mk

