.PHONY : all
all : 
	nasm -f elf idle.asm -o idle.o
	$(LD) --oformat binary -N --Ttext 0 -e 0 idle.o -o idle.raw

.PHONY : clean
clean : 
	rm -f *~ *.o idle.raw

