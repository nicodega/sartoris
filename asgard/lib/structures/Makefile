include ../../include/dep_rule.mk

ASFLAGS = -f elf

dependencies = avltree.d string.d lptree.d list.d sha-1.d stdlibsim.d

all : avltree.o string.o lptree.o list.o sha-1.o stdlibsim.o int64.o

CFLAGS+= -I"../../include"

int64.o : int64.s ../../include/lib/structures/int64.h

.PHONY : clean
clean :
	rm -f *.o *.d *~

include ../../include/dep_attach.mk
