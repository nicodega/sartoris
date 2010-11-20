.PHONY : all
all : 
	nasm -f elf mmx.asm -o mmx.o 
	$(LD) --oformat binary -N --Ttext 0 -e 0 mmx.o -o mmx.raw

.PHONY : clean
clean : 
	rm -f *~ *.o mmx.raw

