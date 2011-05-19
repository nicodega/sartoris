objects = $(extobjects)
extobjects = ../../klib/kserv-user.o

all : idle.raw 

idle.raw: $(objects)
	nasm -f elf idle.asm -o idle.o
	$(LD) --oformat binary -Map idle.map -N --Ttext 0 -e 0 idle.o -o idle.raw $(objects)

.PHONY : clean
clean : 
	rm -f *~ *.o idle.raw

