objects = ../../../mk/kimg.o cpu-arch.o scr-print.o

fill = ../../../utils/fill

cpu_objects = main.o task.o pgrequest.o descriptor.o thread.o tss.o interrupt.o paging.o memory.o cpu-asm.o syscall-gates.o int.o pic.o state_switch.o caps.o

prt_objects = print.o vga.o

dependencies = main.d task.d descriptor.d thread.d tss.d interrupt.d paging.d memory.d print.d caps.d

CPPFLAGS += -I"../../../include/" -I"../include"

%.d : %.c
	@echo making deps for $< ...
	@gcc -E -MD $(CPPFLAGS) $< > /dev/null 

all : kimg 

kimg : $(objects)
	$(LD) $(objects) --oformat binary -Ttext 0x100000 -e 0x100000 -Map kernel.map -o kimg
	$(fill) kimg -m 512
	#Checking Kernel size... (less than 0xF0000
	../size.sh

kelf : $(objects)
	$(LD) $(objects) -o kelf

cpu-arch.o : $(cpu_objects) 
	$(LD) -i $(cpu_objects) -o cpu-arch.o

scr-print.o : $(prt_objects)
	$(LD) -i $(prt_objects) -o scr-print.o

.PHONY : clean
clean : 
	rm -f *.o *.d *.map kimg kelf zeros
	rm -f *~

.PHONY : dep
dep : $(dependencies)

ifeq ($(filter clean dep,$(MAKECMDGOALS)), ) 
include $(dependencies)
endif
