
CPPFLAGS +=  -I"../include" -I"../arch/target/include"

objects = main.o task.o thread.o paging.o interrupt.o containers.o indexing.o salloc.o dynmem.o \
          message.o shared-mem.o bitops.o metrics.o

dependencies = main.d task.d thread.d paging.d interrupt.d metrics.d containers.d indexing.d salloc.d dynmem.d \
               message.d shared-mem.d bitops.d

%.d : %.c
	@echo making deps for $< ...
	@gcc -E -MD $(CPPFLAGS) $< > /dev/null 

all : kimg.o

kimg.o : $(objects)
	$(LD) -i $(objects) -o kimg.o

.PHONY : clean
clean : 
	rm -f *.o *.d
	rm -f *~
	rm -f ../include/sartoris/*~	

clean_all : clean

.PHONY : dep
dep : $(dependencies)

ifeq ($(filter clean clean_all dep,$(MAKECMDGOALS)), ) 
include $(dependencies)
endif
