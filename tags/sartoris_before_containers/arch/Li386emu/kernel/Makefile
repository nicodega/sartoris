CC = gcc
CPPFLAGS += -I"../../../include" -I"../include" -I.
CFLAGS += -O2 
AS = nasm
ASFLAGS = -f elf

dependencies = kernel-emu.d

%.d : %.c
	@echo making deps for $<
	@gcc -E -MD $(CPPFLAGS) $< > /dev/null 

all : arch_kernel

arch_kernel : kernel-emu.o

.PHONY : clean
clean :
	rm -f *~ *.o *.d core

.PHONY : dep
dep : $(dependencies)

ifeq ($(filter clean dep,$(MAKECMDGOALS)), ) 
include $(dependencies)
endif
