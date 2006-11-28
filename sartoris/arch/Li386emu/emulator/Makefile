CC = gcc
CPPFLAGS += -I"../../../include" -I"../include" -I. 
CFLAGS += -O2 
AS = nasm
ASFLAGS = -f elf

objects = main.o syserr.o trampoline.o 

dependencies = main.d syserr.d

%.d : %.c
	@echo making deps for $<
	@gcc -E -MD $(CPPFLAGS) $< > /dev/null 

all : emulator.o

.PHONY : emulator.o
emulator.o : $(objects)
	ld -i $(objects) -o emulator.o

.PHONY : clean
clean :
	rm -f \#* *~ *.o *.d test core

.PHONY : dep
dep : $(dependencies)

ifeq ($(filter clean dep,$(MAKECMDGOALS)), ) 
include $(dependencies)
endif
