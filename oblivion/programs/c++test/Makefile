CPPFLAGS += -I"../../include" -DCPLUSPLUS

objects = test.o printer.o $(extobjects)
extobjects = ../../klib/kserv-user.o ../../lib/scheduler.o ../../lib/malloc.o

all : test.raw

test.raw : $(objects)
	$(LD) --oformat binary --Ttext 0 -o test.raw -N -e 0 \
--defsym __gxx_personality_v0=0 $(objects)

test.o : test.cc printer.h 

printer.o : printer.h printer.cc

.PHONY : clean
clean :
	rm -f *.o *~ test.raw
